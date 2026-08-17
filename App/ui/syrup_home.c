#include "syrup_home.h"

#include <string.h>

#include "app/dtmf.h"
#ifdef ENABLE_AM_FIX
#include "am_fix.h"
#endif
#include "bitmaps.h"
#include "dcs.h"
#include "driver/bk4819.h"
#include "driver/gpio.h"
#include "driver/st7565.h"
#include "external/printf/printf.h"
#include "font.h"
#include "functions.h"
#include "helper/battery.h"
#include "misc.h"
#include "radio.h"
#include "settings.h"
#include "ui/battery.h"
#include "ui/helper.h"
#include "ui/inputbox.h"
#include "ui/ui.h"

/* Layout: two channel rows on top; wave / last-RX / battery at bottom */
#define SH_STATUS_H        8u
#define SH_SCREEN_H        64u
#define SH_WAVE_H          20u
#define SH_CH_H            20u
#define SH_CH_GAP          2u
#define SH_CH0_Y           0u
#define SH_CH1_Y           (SH_CH0_Y + SH_CH_H + SH_CH_GAP)     /* 22 */
#define SH_WAVE_Y          (SH_CH1_Y + SH_CH_H + SH_CH_GAP)     /* 44 */
#define SH_NAME_Y_OFF      0u
#define SH_PARAM_Y_OFF     8u  /* mod/pwr/sql — 4px above former +12 */
#define SH_TONE_Y_OFF      14u /* R:/T: subtones under params */
#define SH_FREQ_Y_OFF      12u /* right-column frequency (unchanged) */
#define SH_RIGHT_X_INSET   2u  /* right column shifted left */
#define SH_GAP_PX          2u

/* RX: stacked blocks — longer (wider) bricks; vertical pack fills SH_WAVE_H */
#define SH_RX_BLOCK_W      5u   /* longer horizontally */
#define SH_RX_BLOCK_H      3u   /* nominal brick height (actual slots pack to fill row) */
#define SH_RX_TIP_H        (SH_RX_BLOCK_H / 3u) /* thin peak cap (1px when H=3) */
#define SH_RX_PITCH_X      6u   /* 5px block + 1px column gap */
#define SH_RX_PITCH_Y      3u   /* nominal; draw uses even pack into SH_WAVE_H */
#define SH_RX_MAX_BLOCKS   ((SH_WAVE_H) / SH_RX_PITCH_Y)
#define SH_RX_COLS         (LCD_WIDTH / SH_RX_PITCH_X)
#define SH_RX_PAUSE_GATE   12u  /* below this dynamics → silence / peak-fall */

/* TX: hairline vertical bars, newest on the left, scroll right */
#define SH_TX_BAR_W        1u
#define SH_TX_GAP          1u
#define SH_TX_PITCH        (SH_TX_BAR_W + SH_TX_GAP)
#define SH_TX_BARS         (LCD_WIDTH / SH_TX_PITCH)
#define SH_TX_MAX_HALF     ((SH_WAVE_H / 2u) - 1u)  /* reach top & bottom of row */

#define SH_TICK_PERIOD_10MS  5u   /* ~50ms — slower overall */
#define SH_RX_TIP_FALL       5u   /* tip brick ~250ms per step */
#define SH_METER_PAD_X       1u
#define SH_METER_PAD_Y       1u
#define SH_METER_TEXT_H      6u   /* gFont3x5 glyph height */

/* Last-RX placeholder: speaker + name (top); battery / lock (bottom) */
#define SH_SPK_W             12u
#define SH_SPK_H             8u
#define SH_SPK_GAP           3u
#define SH_NAME_MAX          10u
#define SH_PH_BAT_H          8u   /* battery / lock / small-font text height */
#define SH_PH_LINE_GAP       2u   /* gap between name row and battery row */
#define SH_PH_ROW_GAP        2u   /* gap between lock, bat, text */

static uint8_t  s_rx_band[SH_RX_COLS]; /* current EQ body height */
static uint8_t  s_rx_tip[SH_RX_COLS];  /* falling peak brick */
static uint8_t  s_rx_phase;
static uint8_t  s_rx_tip_div;
static uint8_t  s_tx_bars[SH_TX_BARS];
static uint8_t  s_tx_hold;             /* louder = hold scroll longer */
static uint16_t s_tx_floor;            /* adaptive silence floor for big swing */
static bool     s_was_tx;
static bool     s_was_rx;
static bool     s_last_rx_valid;
static uint8_t  s_last_rx_vfo;
static uint16_t s_last_rx_channel;
static bool     s_placeholder_blit_done;
static uint8_t  s_tick_div;

/* Megaphone + sound waves (column-major, bit0 = top), from UI speaker asset */
static const uint8_t s_spk_bitmap[SH_SPK_W] = {
	0b00111100,
	0b00111100,
	0b00111100,
	0b00111100,
	0b01111110,
	0b01111110,
	0b11111111,
	0b11111111,
	0b00000000,
	0b01011011,
	0b11011011,
	0b10011001,
};

/* Screen-absolute pixel (0..63): y 0..7 → status line, else framebuffer */
static void draw_pixel(uint8_t x, uint8_t y, bool black)
{
	if (x >= LCD_WIDTH || y >= SH_SCREEN_H)
		return;
	if (y < SH_STATUS_H) {
		const uint8_t pattern = (uint8_t)(1u << (y % 8u));
		if (black)
			gStatusLine[x] |= pattern;
		else
			gStatusLine[x] &= (uint8_t)~pattern;
		return;
	}
	UI_DrawPixelBuffer(gFrameBuffer, x, (uint8_t)(y - SH_STATUS_H), black);
}

static void fill_rect(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, bool black)
{
	for (uint8_t y = y0; y <= y1 && y < SH_SCREEN_H; y++) {
		for (uint8_t x = x0; x <= x1 && x < LCD_WIDTH; x++)
			draw_pixel(x, y, black);
	}
}

#ifdef ENABLE_CHINESE
static bool is_cjk_utf8(const char *p)
{
	const uint8_t c = (uint8_t)p[0];
	return (c >= 0xE4u && c <= 0xEFu);
}

static uint16_t utf8_to_unicode(const char *p)
{
	return (uint16_t)((((uint8_t)p[0] & 0x0Fu) << 12) |
	                   (((uint8_t)p[1] & 0x3Fu) << 6) |
	                   ((uint8_t)p[2] & 0x3Fu));
}

static void draw_cjk_glyph_screen(uint16_t unicode, uint8_t x, uint8_t y_top)
{
	int16_t  spi_index;
	uint16_t spi_bitmap[12];

	spi_index = SETTINGS_CNCharToIndex(unicode);
	if (spi_index < 0)
		return;
	SETTINGS_ReadCNFontBitmap((uint16_t)spi_index, spi_bitmap);
	for (uint8_t row = 0; row < 12u; row++) {
		const uint16_t row_data = spi_bitmap[row];
		for (uint8_t col = 0; col < 12u; col++) {
			if (row_data & (uint16_t)(0x8000u >> col))
				draw_pixel((uint8_t)(x + col), (uint8_t)(y_top + row), true);
		}
	}
}
#endif

static uint8_t small_text_width(const char *text)
{
#ifdef ENABLE_CHINESE
	return (uint8_t)UI_SmallStringPixelWidth(text);
#else
	const uint8_t char_w = (uint8_t)ARRAY_SIZE(gFontSmall[0]);
	const uint8_t pitch  = (uint8_t)(char_w + 1u);
	return (uint8_t)(strlen(text) * pitch);
#endif
}

static uint8_t draw_small_text(const char *text, uint8_t x, uint8_t y_top, bool black)
{
	const uint8_t left = x;
#ifdef ENABLE_CHINESE
	size_t i = 0;
	const bool mixed_cjk = SETTINGS_ChannelNameHasCjkUtf8(text);
	const uint8_t latin_y = mixed_cjk
		? UI_SmallLatinPixelY(y_top, (uint8_t)(y_top + 11u), true, 0u)
		: y_top;
	while (text[i] != '\0') {
		if (is_cjk_utf8(&text[i])) {
			(void)black;
			draw_cjk_glyph_screen(utf8_to_unicode(&text[i]), x, y_top);
			x = (uint8_t)(x + 12u + 1u);
			i += 3u;
		} else {
			const char c = text[i];
			if (c > ' ' && c < 127) {
				const unsigned int index = (unsigned int)(c - ' ' - 1);
				const uint8_t *glyph = gFontSmall[index];
				const uint8_t gx = (uint8_t)(x + 1u);
				const uint8_t char_w = (uint8_t)ARRAY_SIZE(gFontSmall[0]);
				for (uint8_t col = 0; col < char_w; col++) {
					uint8_t bits = glyph[col];
					for (uint8_t row = 0; row < 8u; row++) {
						if (bits & (uint8_t)(1u << row))
							draw_pixel((uint8_t)(gx + col), (uint8_t)(latin_y + row), black);
					}
				}
			}
			x = (uint8_t)(x + (uint8_t)ARRAY_SIZE(gFontSmall[0]) + 1u);
			i++;
		}
	}
#else
	{
	const uint8_t char_w = (uint8_t)ARRAY_SIZE(gFontSmall[0]);
	const uint8_t pitch  = (uint8_t)(char_w + 1u);

	for (size_t i = 0; text[i] != '\0'; i++) {
		const char c = text[i];
		if (c > ' ' && c < 127) {
			const unsigned int index = (unsigned int)(c - ' ' - 1);
			const uint8_t *glyph = gFontSmall[index];
			const uint8_t gx = (uint8_t)(x + 1u);
			for (uint8_t col = 0; col < char_w; col++) {
				uint8_t bits = glyph[col];
				for (uint8_t row = 0; row < 8u; row++) {
					if (bits & (uint8_t)(1u << row))
						draw_pixel((uint8_t)(gx + col), (uint8_t)(y_top + row), black);
				}
			}
		}
		x = (uint8_t)(x + pitch);
	}
	}
#endif
	return left;
}

static uint8_t draw_right_small(const char *text, uint8_t y_top)
{
	const uint8_t w = small_text_width(text);
	const uint8_t inset = SH_RIGHT_X_INSET;
	const uint8_t x = (w + inset >= LCD_WIDTH) ? 0u : (uint8_t)(LCD_WIDTH - w - inset);
	draw_small_text(text, x, y_top, true);
	return x;
}

static uint8_t smallest_width(const char *text)
{
	return (uint8_t)(strlen(text) * 4u);
}

/* 3x5 text at screen-absolute Y (CH0 sits in the hardware status line) */
static void draw_smallest_abs(const char *text, uint8_t x, uint8_t screen_y, bool fill)
{
	if (screen_y < SH_STATUS_H)
		GUI_DisplaySmallest(text, x, screen_y, true, fill);
	else
		GUI_DisplaySmallest(text, x, (uint8_t)(screen_y - SH_STATUS_H), false, fill);
}

static uint8_t draw_param(const char *text, uint8_t x, uint8_t y, bool black)
{
	if (text == NULL || text[0] == '\0')
		return x;
	draw_smallest_abs(text, x, y, black);
	return (uint8_t)(x + smallest_width(text) + SH_GAP_PX);
}

static const char *power_letter(uint8_t power)
{
	static const char *const names[] = {"U", "L1", "L2", "L3", "L4", "L5", "M", "H"};
	if (power >= ARRAY_SIZE(names))
		return "?";
	return names[power];
}

static void format_tone(char *out, size_t out_sz, const FREQ_Config_t *pConfig)
{
	out[0] = '\0';
	if (pConfig == NULL)
		return;
	switch (pConfig->CodeType) {
	case CODE_TYPE_CONTINUOUS_TONE:
		snprintf(out, out_sz, "%u.%u",
		         CTCSS_Options[pConfig->Code] / 10u,
		         CTCSS_Options[pConfig->Code] % 10u);
		break;
	case CODE_TYPE_DIGITAL:
		snprintf(out, out_sz, "%03oN", DCS_Options[pConfig->Code]);
		break;
	case CODE_TYPE_REVERSE_DIGITAL:
		snprintf(out, out_sz, "%03oI", DCS_Options[pConfig->Code]);
		break;
	default:
		break;
	}
}

static void invert_pixel(uint8_t x, uint8_t y)
{
	if (x >= LCD_WIDTH || y >= SH_SCREEN_H)
		return;
	if (y < SH_STATUS_H) {
		gStatusLine[x] ^= (uint8_t)(1u << y);
		return;
	}
	{
		const uint8_t sy = (uint8_t)(y - SH_STATUS_H);
		gFrameBuffer[sy / 8u][x] ^= (uint8_t)(1u << (sy % 8u));
	}
}

static void invert_channel_row(uint8_t vfo)
{
	const uint8_t y0 = (vfo == 0u) ? SH_CH0_Y : SH_CH1_Y;
	const uint8_t y1 = (uint8_t)(y0 + SH_CH_H - 1u);

	if (y0 > 0u) {
		const uint8_t y_bar = (uint8_t)(y0 - 1u);
		for (uint8_t x = 0; x < LCD_WIDTH; x++)
			draw_pixel(x, y_bar, true);
	}

	for (uint8_t y = y0; y <= y1 && y < SH_SCREEN_H; y++) {
		for (uint8_t x = 0; x < LCD_WIDTH; x++)
			invert_pixel(x, y);
	}
}

static void draw_dtmf_live(uint8_t y, uint8_t x_right, const char *digits)
{
	static const char prefix[] = "DTMF:";
	char buf[28];
	size_t dig_off = 0;
	const size_t dig_len = (digits != NULL) ? strlen(digits) : 0;
	const uint8_t x_left = 1u;
	uint8_t avail;

	if (x_right <= (uint8_t)(x_left + SH_GAP_PX))
		return;
	avail = (uint8_t)(x_right - SH_GAP_PX - x_left);

	while (dig_off < dig_len) {
		snprintf(buf, sizeof(buf), "%s%s", prefix, digits + dig_off);
		if (smallest_width(buf) <= avail)
			break;
		dig_off++;
	}
	snprintf(buf, sizeof(buf), "%s%s", prefix, digits + dig_off);
	draw_smallest_abs(buf, x_left, y, true);
}

static void draw_channel_row(uint8_t vfo)
{
	const VFO_Info_t *info = &gEeprom.VfoInfo[vfo];
	const uint8_t top = (vfo == 0u) ? SH_CH0_Y : SH_CH1_Y;
	const uint8_t name_y = (uint8_t)(top + SH_NAME_Y_OFF);
	const uint8_t badge_y = (uint8_t)(top + 1u);
	const uint8_t param_y = (uint8_t)(top + SH_PARAM_Y_OFF);
	const uint8_t tone_y = (uint8_t)(top + SH_TONE_Y_OFF);
	const uint8_t freq_y = (uint8_t)(top + SH_FREQ_Y_OFF);
	const bool transmitting =
		(gCurrentFunction == FUNCTION_TRANSMIT && gEeprom.TX_VFO == vfo);
	const bool show_dtmf =
		gSetting_live_DTMF_decoder &&
		gDTMF_RX_live[0] != 0 &&
		vfo == gEeprom.TX_VFO;

	char String[22];
	char rx_tone[12];
	char tx_tone[12];
	char freq_str[16];
	uint8_t x = 2u;

	/* CH badge (inverted) — top line left; tone moved to row 3 */
	snprintf(String, sizeof(String), "CH%u", (unsigned)(vfo + 1u));
	{
		const uint8_t ch_w = smallest_width(String);
		const uint8_t text_x = x;
		const uint8_t box_x0 = (uint8_t)(text_x - 1u);
		const uint8_t box_x1 = (uint8_t)(text_x + ch_w);
		const uint8_t box_y0 = (uint8_t)(badge_y - 1u);
		const uint8_t box_y1 = (uint8_t)(badge_y + 5u);
		fill_rect(box_x0, box_y0, box_x1, box_y1, true);
		draw_smallest_abs(String, text_x, badge_y, false);
	}

	/* frequency string (right column, also DTMF bound) */
	if (gInputBoxIndex > 0 && gEeprom.TX_VFO == vfo) {
		snprintf(freq_str, sizeof(freq_str), "%s", INPUTBOX_GetAscii());
	} else {
		uint32_t frequency = transmitting ? info->pTX->Frequency : info->pRX->Frequency;
		if (info->TX_OFFSET_FREQUENCY_DIRECTION == TX_OFFSET_FREQUENCY_DIRECTION_ADD)
			snprintf(freq_str, sizeof(freq_str), "+%03u.%05u",
			         frequency / 100000u, frequency % 100000u);
		else if (info->TX_OFFSET_FREQUENCY_DIRECTION == TX_OFFSET_FREQUENCY_DIRECTION_SUB)
			snprintf(freq_str, sizeof(freq_str), "-%03u.%05u",
			         frequency / 100000u, frequency % 100000u);
		else
			snprintf(freq_str, sizeof(freq_str), "%03u.%05u",
			         frequency / 100000u, frequency % 100000u);
	}

	if (show_dtmf) {
		draw_dtmf_live(param_y,
		               (uint8_t)(LCD_WIDTH - SH_RIGHT_X_INSET - small_text_width(freq_str)),
		               gDTMF_RX_live);
	} else {
		/* row 2: modulation, power, squelch */
		x = 1u;
		x = draw_param(gModulationStr[info->Modulation], x, param_y, true);
		x = draw_param(power_letter(info->OUTPUT_POWER), x, param_y, true);
		snprintf(String, sizeof(String), "%u", (unsigned)gEeprom.SQUELCH_LEVEL);
		draw_param(String, x, param_y, true);

		/* row 3: RX + TX subtones (omit when OFF) */
		format_tone(rx_tone, sizeof(rx_tone), info->pRX);
		format_tone(tx_tone, sizeof(tx_tone), info->pTX);
		x = 1u;
		if (rx_tone[0] != '\0') {
			snprintf(String, sizeof(String), "R:%s", rx_tone);
			x = draw_param(String, x, tone_y, true);
		}
		if (tx_tone[0] != '\0') {
			snprintf(String, sizeof(String), "T:%s", tx_tone);
			draw_param(String, x, tone_y, true);
		}
	}

	/* name / VFO state on the right of the top line */
	if (VfoState[vfo] != VFO_STATE_NORMAL &&
	    VfoState[vfo] < _VFO_STATE_LAST_ELEMENT &&
	    VfoStateStr[VfoState[vfo]] != NULL &&
	    VfoStateStr[VfoState[vfo]][0] != '\0') {
		snprintf(String, sizeof(String), "%s", VfoStateStr[VfoState[vfo]]);
	} else {
		SETTINGS_FetchChannelName(String, gEeprom.ScreenChannel[vfo]);
		if (String[0] == 0) {
			if (IS_MR_CHANNEL(gEeprom.ScreenChannel[vfo]))
				snprintf(String, sizeof(String), "CH-%04u",
				         gEeprom.ScreenChannel[vfo] + 1u);
			else
				snprintf(String, sizeof(String), "VFO-%u", (unsigned)(vfo + 1u));
		}
	}
#ifdef ENABLE_CHINESE
	String[CHANNEL_NAME_MAX_BYTES] = 0;
#else
	String[10] = 0;
#endif
	{
		const uint8_t name_left = draw_right_small(String, name_y);
		if (IS_MR_CHANNEL(gEeprom.ScreenChannel[vfo])) {
			char num[6];
			snprintf(num, sizeof(num), "%u",
			         (unsigned)(gEeprom.ScreenChannel[vfo] + 1u));
			const uint8_t num_w = smallest_width(num);
			if (num_w + 2u < name_left)
				draw_smallest_abs(num, (uint8_t)(name_left - 2u - num_w), name_y, true);
		}
	}

	draw_right_small(freq_str, freq_y);
}

/* TX: map mic amplitude onto full half-row height with large dynamic range */
static uint8_t voice_to_tx_half(uint16_t amp)
{
	if (amp < 50u)
		amp = 50u;

	/* floor tracks quiet level: drops fast, rises slowly */
	if (amp < s_tx_floor)
		s_tx_floor = amp;
	else if (s_tx_floor + 4u < amp)
		s_tx_floor = (uint16_t)(s_tx_floor + 4u);
	else if (s_tx_floor < amp)
		s_tx_floor++;

	uint16_t span = (amp > s_tx_floor) ? (uint16_t)(amp - s_tx_floor) : 0u;
	if (span < 8u)
		return 0u; /* near silence → baseline only */

	/*
	 * Aggressive curve into 1..SH_TX_MAX_HALF so speech swings hard
	 * and peaks can fill the whole wave row (top + bottom).
	 */
	uint32_t h = ((uint32_t)(span - 8u) * SH_TX_MAX_HALF) / 180u;
	if (h < 1u)
		h = 1u;
	if (h > SH_TX_MAX_HALF)
		h = SH_TX_MAX_HALF;
	return (uint8_t)h;
}

static uint8_t energy_to_blocks(uint16_t energy)
{
	/* energy is roughly 0..400 → 0..SH_RX_MAX_BLOCKS */
	static const uint16_t thresholds[] = {
		20, 40, 70, 110, 160, 220, 290, 370
	};
	uint8_t level = 0;
	for (uint8_t i = 0; i < ARRAY_SIZE(thresholds) && i < SH_RX_MAX_BLOCKS; i++) {
		if (energy >= thresholds[i])
			level = (uint8_t)(i + 1u);
	}
	if (level > SH_RX_MAX_BLOCKS)
		level = SH_RX_MAX_BLOCKS;
	return level;
}

static void reset_wave_state(void)
{
	memset(s_rx_band, 0, sizeof(s_rx_band));
	memset(s_rx_tip, 0, sizeof(s_rx_tip));
	memset(s_tx_bars, 0, sizeof(s_tx_bars));
	s_rx_phase = 0;
	s_rx_tip_div = 0;
	s_tx_hold = 0;
	s_tx_floor = 200u;
}

static bool rx_pillars_alive(void)
{
	for (uint8_t i = 0; i < SH_RX_COLS; i++) {
		if (s_rx_band[i] > 0u || s_rx_tip[i] > 0u)
			return true;
	}
	return false;
}

/* Sound stopped: body vanishes at once; only the top brick falls slowly */
static void sample_rx_decay(void)
{
	for (uint8_t i = 0; i < SH_RX_COLS; i++) {
		if (s_rx_band[i] > s_rx_tip[i])
			s_rx_tip[i] = s_rx_band[i];
		s_rx_band[i] = 0u; /* lower blocks disappear immediately */
	}

	if (++s_rx_tip_div >= SH_RX_TIP_FALL) {
		s_rx_tip_div = 0;
		for (uint8_t i = 0; i < SH_RX_COLS; i++) {
			if (s_rx_tip[i] > 0u)
				s_rx_tip[i]--;
		}
	}
}

static void sample_rx_wave(void)
{
	/*
	 * Chaotic EQ pillars while audio is present; on silence freeze body
	 * and let the top brick fall slowly (peak-hold decay).
	 */
	const uint8_t  noise  = BK4819_GetExNoiceIndicator();
	const uint8_t  glitch = BK4819_GetGlitchIndicator();
	const uint8_t  af     = BK4819_GetAfTxRx();

	uint16_t dynamics = (uint16_t)af + (uint16_t)glitch;
	if (noise < 90u)
		dynamics = (uint16_t)(dynamics + ((90u - noise) >> 1));

	uint16_t activity = dynamics;
	if (activity > 255u)
		activity = 255u;

	const bool paused = (dynamics < SH_RX_PAUSE_GATE);
	if (paused) {
		sample_rx_decay();
		return;
	}

	s_rx_phase++;

	for (uint8_t col = 0; col < SH_RX_COLS; col++) {
		const uint16_t h = (uint16_t)(
			((uint16_t)s_rx_phase * 37u) ^
			((uint16_t)col * 157u) ^
			((uint16_t)(s_rx_phase + col) * 13u));
		const uint8_t mix = (uint8_t)(h ^ (h >> 8));

		uint16_t target = (uint16_t)(((uint32_t)activity * (40u + (mix % 216u))) / 255u);

		if ((mix & 0x07u) == 0u)
			target = (uint16_t)(target + (activity >> 1));
		else if ((mix & 0x07u) == 1u)
			target >>= 2;
		else if ((mix & 0x0Fu) == 2u)
			target >>= 1;

		{
			const int8_t wobble = (int8_t)(((mix >> 3) & 7u) - 3);
			int16_t t = (int16_t)target + (int16_t)wobble * (int16_t)(activity >> 6);
			if (t < 0)
				t = 0;
			if (t > 400)
				t = 400;
			target = (uint16_t)t;
		}

		const uint8_t want = energy_to_blocks(target);

		/* slow attack / release — one step per tick max */
		if (want > s_rx_band[col])
			s_rx_band[col]++;
		else if (want < s_rx_band[col] && s_rx_band[col] > 0u)
			s_rx_band[col]--;

		if (s_rx_band[col] > s_rx_tip[col])
			s_rx_tip[col] = s_rx_band[col];
	}

	/* while loud, tip still slowly settles toward body if it overshot */
	if (++s_rx_tip_div >= SH_RX_TIP_FALL) {
		s_rx_tip_div = 0;
		for (uint8_t i = 0; i < SH_RX_COLS; i++) {
			if (s_rx_tip[i] > s_rx_band[i])
				s_rx_tip[i]--;
		}
	}
}

static void sample_tx_wave(void)
{
	uint16_t amp = BK4819_GetVoiceAmplitudeOut();
	if (amp == 0u)
		amp = 200u;

	if (s_tx_hold > 0u) {
		s_tx_hold--;
		return; /* pause scroll while loud — “根据大小停顿” */
	}

	/* shift right; insert newest at left */
	for (int i = (int)SH_TX_BARS - 1; i > 0; i--)
		s_tx_bars[i] = s_tx_bars[i - 1];
	s_tx_bars[0] = voice_to_tx_half(amp);

	/* brief hold only on strong peaks so the scroll stays lively */
	if (s_tx_bars[0] >= (SH_TX_MAX_HALF - 1u))
		s_tx_hold = 1u;
	else
		s_tx_hold = 0u;
}

static void clear_wave_area(void)
{
	const uint8_t y1 = (uint8_t)(SH_WAVE_Y + SH_WAVE_H - 1u);
	fill_rect(0, SH_WAVE_Y, LCD_WIDTH - 1u, y1, false);
}

/* Center a short hint in the bar/wave row (low battery / keypad unlock) */
static void draw_wave_centered_message(const char *text, bool key_lock_hint)
{
	uint8_t name_w;
	uint8_t text_h;
	uint8_t x;
	uint8_t y;

	(void)key_lock_hint;
	clear_wave_area();

#ifdef ENABLE_CHINESE
	text_h = UI_SmallLinePixelHeight(text);
#else
	text_h = 8u;
#endif
	name_w = small_text_width(text);
	if (name_w >= LCD_WIDTH)
		x = 0u;
	else
		x = (uint8_t)((LCD_WIDTH - name_w) / 2u);
	y = (uint8_t)(SH_WAVE_Y + (SH_WAVE_H - text_h) / 2u);
	draw_small_text(text, x, y, true);
}

static bool wave_overlay_active(void)
{
	if (gLowBattery && !gLowBatteryConfirmed)
		return true;
	if (gEeprom.KEY_LOCK && gKeypadLocked > 0)
		return true;
	return false;
}

static void draw_wave_overlay(void)
{
	if (gLowBattery && !gLowBatteryConfirmed) {
		const char *msg = "LOW BATTERY";
#ifdef ENABLE_CHINESE
		if (gUiLanguage == UI_LANGUAGE_CN)
			msg = "\xe4\xbd\x8e\xe7\x94\xb5\xe9\x87\x8f"; /* 低电量 */
#endif
		draw_wave_centered_message(msg, false);
		return;
	}

	if (gEeprom.KEY_LOCK && gKeypadLocked > 0) {
		const char *msg = "UNLOCK KEYBOARD";
#ifdef ENABLE_CHINESE
		if (gUiLanguage == UI_LANGUAGE_CN)
			msg = "\xe9\x95\xbf\xe6\x8c\x89#\xe8\xa7\xa3\xe9\x94\x81"; /* 长按#解锁 */
#endif
		draw_wave_centered_message(msg, true);
	}
}

/* Pack block b (0=bottom) into the full wave row so max height reaches SH_WAVE_Y */
static void rx_block_ys(uint8_t b_from_bottom, uint8_t *y0, uint8_t *y1)
{
	const uint8_t bot = (uint8_t)(SH_WAVE_Y + SH_WAVE_H - 1u);
	const uint16_t lo = ((uint16_t)b_from_bottom * SH_WAVE_H) / SH_RX_MAX_BLOCKS;
	const uint16_t hi = ((uint16_t)(b_from_bottom + 1u) * SH_WAVE_H) / SH_RX_MAX_BLOCKS;

	*y1 = (uint8_t)(bot - lo);
	*y0 = (uint8_t)(bot - (hi - 1u));
}

/* Wave-row center S / dBm — tight plate only */
static void draw_s_meter_label(void)
{
	char buf[20];
	int16_t rssi_dBm;
	uint8_t s_level;
	uint8_t text_w;
	uint8_t text_x;
	uint8_t text_y;
	uint8_t box_x0;
	uint8_t box_x1;
	uint8_t box_y0;
	uint8_t box_y1;

	if (!FUNCTION_IsRx())
		return;

	rssi_dBm = BK4819_GetRSSI_dBm();
#ifdef ENABLE_AM_FIX
	if (gSetting_AM_fix && gRxVfo->Modulation == MODULATION_AM)
		rssi_dBm = (int16_t)(rssi_dBm + AM_fix_get_gain_diff());
#endif
	{
		const unsigned int b = gEeprom.VfoInfo[gEeprom.RX_VFO].Band;
		if (b < 7u)
			rssi_dBm = (int16_t)(rssi_dBm + dBmCorrTable[b]);
	}

	/* IARU VHF/UHF: S9 = -93 dBm, 6 dB per S-unit */
	if (rssi_dBm >= -93)
		s_level = 9u;
	else if (rssi_dBm < -141)
		s_level = 0u;
	else
		s_level = (uint8_t)((rssi_dBm + 147) / 6);

	snprintf(buf, sizeof(buf), "S%u %d dBm", (unsigned)s_level, (int)rssi_dBm);

	text_w = smallest_width(buf);
	if (text_w >= LCD_WIDTH)
		text_x = 0u;
	else
		text_x = (uint8_t)((LCD_WIDTH - text_w) / 2u);

	/* flush under the top of the wave row; only a small center strip */
	text_y = (uint8_t)(SH_WAVE_Y + SH_METER_PAD_Y);

	box_x0 = (text_x > SH_METER_PAD_X) ? (uint8_t)(text_x - SH_METER_PAD_X) : 0u;
	box_x1 = (uint8_t)(text_x + text_w + SH_METER_PAD_X);
	if (box_x1 >= LCD_WIDTH)
		box_x1 = LCD_WIDTH - 1u;
	box_y0 = SH_WAVE_Y;
	box_y1 = (uint8_t)(text_y + SH_METER_TEXT_H - 1u);
	if (box_y1 >= (uint8_t)(SH_WAVE_Y + SH_WAVE_H))
		box_y1 = (uint8_t)(SH_WAVE_Y + SH_WAVE_H - 1u);

	/* tight blank plate only behind the digits — not a full-width row */
	fill_rect(box_x0, box_y0, box_x1, box_y1, false);
	draw_smallest_abs(buf, text_x, text_y, true);
}

static void draw_rx_tip_cap(uint8_t x0, uint8_t tip_level)
{
	uint8_t slot_y0;
	uint8_t slot_y1;
	uint8_t tip_y1;

	if (tip_level == 0u || SH_RX_TIP_H == 0u)
		return;

	rx_block_ys((uint8_t)(tip_level - 1u), &slot_y0, &slot_y1);

	/* thin tip flush with the top of this slot (stays inside the wave row) */
	tip_y1 = (uint8_t)(slot_y0 + SH_RX_TIP_H - 1u);
	if (tip_y1 > slot_y1)
		tip_y1 = slot_y1;

	fill_rect(x0, slot_y0, (uint8_t)(x0 + SH_RX_BLOCK_W - 1u), tip_y1, true);
}

static void draw_rx_wave(void)
{
	/* redraw frame (erase previous bricks); state is not wiped on silence */
	clear_wave_area();

	for (uint8_t col = 0; col < SH_RX_COLS; col++) {
		const uint8_t x0   = (uint8_t)(col * SH_RX_PITCH_X);
		const uint8_t body = s_rx_band[col];
		const uint8_t tip  = s_rx_tip[col];

		if (body > 0u) {
			/* solid pillar while audio present — max body fills the whole row */
			for (uint8_t b = 0; b < body; b++) {
				uint8_t y0;
				uint8_t y1;
				rx_block_ys(b, &y0, &y1);
				fill_rect(x0, y0, (uint8_t)(x0 + SH_RX_BLOCK_W - 1u), y1, true);
			}
			/* thin peak cap on the pillar */
			if (tip > 0u)
				draw_rx_tip_cap(x0, tip);
			else
				draw_rx_tip_cap(x0, body);
		} else if (tip > 0u) {
			/* silence: only the thin peak rectangle slowly descending */
			draw_rx_tip_cap(x0, tip);
		}
	}
}

static void draw_tx_wave(void)
{
	const uint8_t mid = (uint8_t)(SH_WAVE_Y + SH_WAVE_H / 2u);

	clear_wave_area(); /* only TX clears the wave row to blank first */

	/* baseline */
	for (uint8_t x = 0; x < LCD_WIDTH; x++)
		draw_pixel(x, mid, true);

	/* 1px hairline bars, 1px gap — height reaches full wave row */
	for (uint8_t i = 0; i < SH_TX_BARS; i++) {
		const uint8_t h = s_tx_bars[i];
		if (h == 0u)
			continue;
		const uint8_t x = (uint8_t)(i * SH_TX_PITCH);
		for (uint8_t dy = 1; dy <= h; dy++) {
			draw_pixel(x, (uint8_t)(mid - dy), true);
			draw_pixel(x, (uint8_t)(mid + dy), true);
		}
	}
}

static void capture_last_rx(void)
{
	s_last_rx_vfo = gEeprom.RX_VFO;
	if (s_last_rx_vfo > 1u)
		s_last_rx_vfo = 0u;
	s_last_rx_channel = gEeprom.ScreenChannel[s_last_rx_vfo];
	s_last_rx_valid = true;
	s_placeholder_blit_done = false;
}

static void format_last_rx_name(char *out, size_t out_sz)
{
	SETTINGS_FetchChannelName(out, s_last_rx_channel);
	if (out[0] == '\0') {
		if (IS_MR_CHANNEL(s_last_rx_channel))
			snprintf(out, out_sz, "CH-%04u",
			         (unsigned)(s_last_rx_channel + 1u));
		else
			snprintf(out, out_sz, "VFO-%u",
			         (unsigned)(s_last_rx_vfo + 1u));
	}
#ifdef ENABLE_CHINESE
	if (out_sz > CHANNEL_NAME_MAX_BYTES)
		out[CHANNEL_NAME_MAX_BYTES] = '\0';
	else if (out_sz > 0)
		out[out_sz - 1u] = '\0';
#else
	out[SH_NAME_MAX] = '\0';
#endif
}

static void draw_spk_bitmap(uint8_t x, uint8_t y_top)
{
	for (uint8_t col = 0; col < SH_SPK_W; col++) {
		const uint8_t bits = s_spk_bitmap[col];
		for (uint8_t row = 0; row < SH_SPK_H; row++) {
			if (bits & (uint8_t)(1u << row))
				draw_pixel((uint8_t)(x + col), (uint8_t)(y_top + row), true);
		}
	}
}

/* Column-major 8px-tall glyph/icon into screen pixels (status-style bitmaps) */
static void draw_col_bitmap(uint8_t x, uint8_t y_top, const uint8_t *cols, uint8_t w)
{
	for (uint8_t c = 0; c < w; c++) {
		const uint8_t bits = cols[c];
		for (uint8_t row = 0; row < 8u; row++) {
			if (bits & (uint8_t)(1u << row))
				draw_pixel((uint8_t)(x + c), (uint8_t)(y_top + row), true);
		}
	}
}

/* Build lock + battery + BatTxt string; returns total width. bat_bmp must be sized. */
static uint8_t prepare_battery_row(uint8_t *bat_bmp, char *bat_str, size_t bat_str_sz,
                                   uint8_t *lock_w_out, uint8_t *bat_w_out, uint8_t *text_w_out)
{
	uint8_t lock_w;
	uint8_t bat_w;
	uint8_t text_w;
	uint8_t total;

	UI_DrawBattery(bat_bmp, gBatteryDisplayLevel, gLowBatteryBlink);
	bat_w = (uint8_t)sizeof(BITMAP_BatteryLevel1);
	lock_w = (gEeprom.KEY_LOCK != 0) ? (uint8_t)sizeof(gFontKeyLock) : 0u;

	bat_str[0] = '\0';
	text_w = 0u;
	switch (gSetting_battery_text) {
	case 1: {
		const uint16_t voltage = MIN(gBatteryVoltageAverage, 999);
		snprintf(bat_str, bat_str_sz, "%u.%02u",
		         voltage / 100u, voltage % 100u);
		text_w = small_text_width(bat_str);
		break;
	}
	case 2:
		snprintf(bat_str, bat_str_sz, "%02u%%",
		         BATTERY_VoltsToPercent(gBatteryVoltageAverage));
		text_w = small_text_width(bat_str);
		break;
	default:
		break;
	}

	total = bat_w;
	if (lock_w > 0u)
		total = (uint8_t)(total + lock_w + SH_PH_ROW_GAP);
	if (text_w > 0u)
		total = (uint8_t)(total + SH_PH_ROW_GAP + text_w);

	*lock_w_out = lock_w;
	*bat_w_out = bat_w;
	*text_w_out = text_w;
	return total;
}

/* One centered line: [lock?] battery [voltage|percent] */
static void draw_battery_status_row(uint8_t y_top)
{
	char bat_str[8];
	uint8_t bat_bmp[sizeof(BITMAP_BatteryLevel1)];
	uint8_t lock_w;
	uint8_t bat_w;
	uint8_t text_w;
	uint8_t total_w;
	uint8_t bx;

	total_w = prepare_battery_row(bat_bmp, bat_str, sizeof(bat_str),
	                              &lock_w, &bat_w, &text_w);

	if (total_w >= LCD_WIDTH)
		bx = 0u;
	else
		bx = (uint8_t)((LCD_WIDTH - total_w) / 2u);

	if (lock_w > 0u) {
		draw_col_bitmap(bx, y_top, gFontKeyLock, lock_w);
		bx = (uint8_t)(bx + lock_w + SH_PH_ROW_GAP);
	}
	draw_col_bitmap(bx, y_top, bat_bmp, bat_w);
	bx = (uint8_t)(bx + bat_w);
	if (text_w > 0u) {
		bx = (uint8_t)(bx + SH_PH_ROW_GAP);
		/* Same gFontSmall as status-bar BatTxt — matches battery icon height */
		draw_small_text(bat_str, bx, y_top, true);
	}
}

/* No last-RX yet (e.g. fresh boot): single centered battery / lock row */
static void draw_idle_battery_placeholder(void)
{
	clear_wave_area();
	draw_battery_status_row((uint8_t)(SH_WAVE_Y + (SH_WAVE_H - SH_PH_BAT_H) / 2u));
}

/* Icon + channel name on top; battery (+ optional lock) centered below.
 * Both rows are vertically centered as one block inside the wave band. */
static void draw_last_rx_placeholder(void)
{
	char name[22];
	uint8_t name_w;
	uint8_t total_w;
	uint8_t x;
	uint8_t name_y;
	uint8_t bat_y;
	uint8_t group_h;
	uint8_t line_gap;
	uint8_t block_h;

	if (!s_last_rx_valid) {
		draw_idle_battery_placeholder();
		return;
	}

	format_last_rx_name(name, sizeof(name));
#ifdef ENABLE_CHINESE
	group_h = UI_SmallLinePixelHeight(name);
	if (group_h <= 8u)
		group_h = (SH_SPK_H > 8u) ? SH_SPK_H : 8u;
#else
	group_h = (SH_SPK_H > 8u) ? SH_SPK_H : 8u; /* gFontSmall is 8px */
#endif
	name_w = small_text_width(name);
	total_w = (uint8_t)(SH_SPK_W + SH_SPK_GAP + name_w);

	/* Vertical center of (name row + gap + battery row) inside SH_WAVE_H */
	line_gap = SH_PH_LINE_GAP;
	block_h = (uint8_t)(group_h + SH_PH_BAT_H);
	if ((uint8_t)(block_h + line_gap) > SH_WAVE_H) {
		line_gap = (block_h >= SH_WAVE_H) ? 0u : (uint8_t)(SH_WAVE_H - block_h);
	}
	block_h = (uint8_t)(group_h + line_gap + SH_PH_BAT_H);
	name_y = (uint8_t)(SH_WAVE_Y + (SH_WAVE_H - block_h) / 2u);
	bat_y = (uint8_t)(name_y + group_h + line_gap);

	clear_wave_area();

	if (total_w >= LCD_WIDTH)
		x = 0u;
	else
		x = (uint8_t)((LCD_WIDTH - total_w) / 2u);
	draw_spk_bitmap(x, name_y);
	draw_small_text(name, (uint8_t)(x + SH_SPK_W + SH_SPK_GAP), name_y, true);

	draw_battery_status_row(bat_y);
}

static void draw_wave_row(void)
{
	if (wave_overlay_active()) {
		draw_wave_overlay();
		return;
	}

	if (gCurrentFunction == FUNCTION_TRANSMIT) {
		draw_tx_wave();
	} else if (FUNCTION_IsRx() || rx_pillars_alive()) {
		/* RX or tip-fall: pillars; S-meter only while actually receiving */
		draw_rx_wave();
		draw_s_meter_label();
	} else if (s_last_rx_valid) {
		draw_last_rx_placeholder();
	} else {
		draw_idle_battery_placeholder();
	}
}

static void blit_wave_lines(void)
{
	uint8_t y = SH_WAVE_Y;
	const uint8_t y_end = (uint8_t)(SH_WAVE_Y + SH_WAVE_H);

	while (y < y_end && y < SH_SCREEN_H) {
		if (y < SH_STATUS_H) {
			ST7565_BlitStatusLine();
			y = SH_STATUS_H;
			continue;
		}
		{
			const uint8_t line = (uint8_t)((y - SH_STATUS_H) / 8u);
			ST7565_BlitLine(line);
			y = (uint8_t)(SH_STATUS_H + (uint8_t)((line + 1u) * 8u));
		}
	}
}

void UI_SyrupHome_Tick10ms(void)
{
	if (gScreenToDisplay != DISPLAY_MAIN)
		return;
	/* Keep overlay text stable; do not animate bars underneath */
	if (wave_overlay_active())
		return;

	if (++s_tick_div < SH_TICK_PERIOD_10MS)
		return;
	s_tick_div = 0;

	const bool tx = (gCurrentFunction == FUNCTION_TRANSMIT);
	const bool rx = FUNCTION_IsRx();

	if (tx && !GPIO_IsPttPressed()
#ifdef ENABLE_VOX
	    && !gEeprom.VOX_SWITCH
#endif
	) {
		/* ignore spurious TX samples when PTT already released */
		return;
	}

	/* only entering TX wipes RX pillar state */
	if (tx && !s_was_tx) {
		reset_wave_state();
		s_placeholder_blit_done = false;
	}
	s_was_tx = tx;

	if (rx && !s_was_rx)
		capture_last_rx();
	s_was_rx = rx;

	if (tx) {
		sample_tx_wave();
	} else if (rx) {
		sample_rx_wave();
		s_placeholder_blit_done = false;
	} else if (rx_pillars_alive()) {
		/* no signal / left RX — keep falling tips, do not clear */
		sample_rx_decay();
		s_placeholder_blit_done = false;
	}
	/* idle: last-RX or boot battery row — keep redrawing so bat / lock stay live */

	draw_wave_row();
	blit_wave_lines();

	if (!tx && !rx && !rx_pillars_alive())
		s_placeholder_blit_done = true;
}

void UI_DisplaySyrupHome(void)
{
	UI_StatusClear();
	UI_DisplayClear();

	/* full-screen clear is fine on page entry; do not zero pillar state unless TX */
	if (gCurrentFunction == FUNCTION_TRANSMIT && !s_was_tx)
		reset_wave_state();

	/* page redraw always refreshes placeholder if shown */
	s_placeholder_blit_done = false;

	draw_wave_row();
	draw_channel_row(0);
	draw_channel_row(1);
	invert_channel_row(gEeprom.TX_VFO);

	if (gCurrentFunction != FUNCTION_TRANSMIT &&
	    !FUNCTION_IsRx() &&
	    !rx_pillars_alive())
		s_placeholder_blit_done = true;
}
