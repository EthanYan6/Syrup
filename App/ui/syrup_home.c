#include "syrup_home.h"

#include <string.h>

#include "app/dtmf.h"
#ifdef ENABLE_AM_FIX
#include "am_fix.h"
#endif
#include "dcs.h"
#include "driver/bk4819.h"
#include "driver/gpio.h"
#include "driver/st7565.h"
#include "external/printf/printf.h"
#include "font.h"
#include "functions.h"
#include "misc.h"
#include "radio.h"
#include "settings.h"
#include "ui/helper.h"
#include "ui/inputbox.h"
#include "ui/ui.h"

/* Layout: 56px framebuffer — wave on top, two 16px channel rows below */
#define SH_WAVE_Y          0u
#define SH_WAVE_H          20u
#define SH_CH_H            16u
#define SH_CH_GAP          2u
#define SH_CH0_Y           22u
#define SH_CH1_Y           (SH_CH0_Y + SH_CH_H + SH_CH_GAP)
#define SH_FB_H            ((uint8_t)(FRAME_LINES * 8u))
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

static uint8_t  s_rx_band[SH_RX_COLS]; /* current EQ body height */
static uint8_t  s_rx_tip[SH_RX_COLS];  /* falling peak brick */
static uint8_t  s_rx_phase;
static uint8_t  s_rx_tip_div;
static uint8_t  s_tx_bars[SH_TX_BARS];
static uint8_t  s_tx_hold;             /* louder = hold scroll longer */
static uint16_t s_tx_floor;            /* adaptive silence floor for big swing */
static bool     s_was_tx;
static uint8_t  s_tick_div;

static void draw_pixel(uint8_t x, uint8_t y, bool black)
{
	if (x >= LCD_WIDTH || y >= SH_FB_H)
		return;
	UI_DrawPixelBuffer(gFrameBuffer, x, y, black);
}

static void fill_rect(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, bool black)
{
	for (uint8_t y = y0; y <= y1 && y < SH_FB_H; y++) {
		for (uint8_t x = x0; x <= x1 && x < LCD_WIDTH; x++)
			draw_pixel(x, y, black);
	}
}

static uint8_t small_text_width(const char *text)
{
	const uint8_t char_w = (uint8_t)ARRAY_SIZE(gFontSmall[0]);
	const uint8_t pitch  = (uint8_t)(char_w + 1u);
	return (uint8_t)(strlen(text) * pitch);
}

static uint8_t draw_small_text(const char *text, uint8_t x, uint8_t y_top, bool black)
{
	const uint8_t char_w = (uint8_t)ARRAY_SIZE(gFontSmall[0]);
	const uint8_t pitch  = (uint8_t)(char_w + 1u);
	const uint8_t left   = x;

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
	return left;
}

static uint8_t draw_right_small(const char *text, uint8_t y_top)
{
	const uint8_t w = small_text_width(text);
	const uint8_t x = (w >= LCD_WIDTH) ? 0u : (uint8_t)(LCD_WIDTH - w);
	draw_small_text(text, x, y_top, true);
	return x;
}

static uint8_t smallest_width(const char *text)
{
	return (uint8_t)(strlen(text) * 4u);
}

static uint8_t draw_param(const char *text, uint8_t x, uint8_t y, bool black)
{
	if (text == NULL || text[0] == '\0')
		return x;
	GUI_DisplaySmallest(text, x, y, false, black);
	return (uint8_t)(x + smallest_width(text) + SH_GAP_PX);
}

static const char *power_letter(uint8_t power)
{
	static const char *const names[] = {"U", "L1", "L2", "L3", "L4", "L5", "M", "H"};
	if (power >= ARRAY_SIZE(names))
		return "?";
	return names[power];
}

static void format_tx_tone(char *out, size_t out_sz, const VFO_Info_t *vfo)
{
	const FREQ_Config_t *pConfig = vfo->pTX;
	out[0] = '\0';
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

static void invert_channel_row(uint8_t vfo)
{
	const uint8_t y0 = (vfo == 0u) ? SH_CH0_Y : SH_CH1_Y;
	const uint8_t y1 = (uint8_t)(y0 + SH_CH_H - 1u);

	if (y0 > 0u) {
		const uint8_t y_bar = (uint8_t)(y0 - 1u);
		for (uint8_t x = 0; x < LCD_WIDTH; x++)
			draw_pixel(x, y_bar, true);
	}

	for (uint8_t y = y0; y <= y1 && y < SH_FB_H; y++) {
		for (uint8_t x = 0; x < LCD_WIDTH; x++) {
			const uint8_t pattern = (uint8_t)(1u << (y % 8u));
			gFrameBuffer[y / 8u][x] ^= pattern;
		}
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
	GUI_DisplaySmallest(buf, x_left, y, false, true);
}

static void draw_channel_row(uint8_t vfo)
{
	const VFO_Info_t *info = &gEeprom.VfoInfo[vfo];
	const uint8_t top = (vfo == 0u) ? SH_CH0_Y : SH_CH1_Y;
	const uint8_t y0  = (uint8_t)(top + 1u);
	const uint8_t y1  = (uint8_t)(top + 8u);
	const bool transmitting =
		(gCurrentFunction == FUNCTION_TRANSMIT && gEeprom.TX_VFO == vfo);
	const bool show_dtmf =
		gSetting_live_DTMF_decoder &&
		gDTMF_RX_live[0] != 0 &&
		vfo == gEeprom.TX_VFO;

	char String[22];
	char tone[12];
	char freq_str[16];
	uint8_t x = 2u;

	/* CH badge (inverted) */
	snprintf(String, sizeof(String), "CH%u", (unsigned)(vfo + 1u));
	{
		const uint8_t ch_w = smallest_width(String);
		const uint8_t text_x = x;
		const uint8_t box_x0 = (uint8_t)(text_x - 1u);
		const uint8_t box_x1 = (uint8_t)(text_x + ch_w);
		const uint8_t box_y0 = (uint8_t)(y0 - 1u);
		const uint8_t box_y1 = (uint8_t)(y0 + 5u);
		fill_rect(box_x0, box_y0, box_x1, box_y1, true);
		GUI_DisplaySmallest(String, text_x, y0, false, false);
		x = (uint8_t)(box_x1 + 1u + SH_GAP_PX);
	}

	format_tx_tone(tone, sizeof(tone), info);
	if (tone[0] != '\0')
		draw_param(tone, x, y0, true);

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
		draw_dtmf_live(y1, (uint8_t)(LCD_WIDTH - small_text_width(freq_str)), gDTMF_RX_live);
	} else {
		x = 1u;
		x = draw_param(gModulationStr[info->Modulation], x, y1, true);
		x = draw_param(power_letter(info->OUTPUT_POWER), x, y1, true);
		snprintf(String, sizeof(String), "%u", (unsigned)gEeprom.SQUELCH_LEVEL);
		draw_param(String, x, y1, true);
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
	String[10] = 0;
	{
		const uint8_t name_left = draw_right_small(String, top);
		if (IS_MR_CHANNEL(gEeprom.ScreenChannel[vfo])) {
			char num[6];
			snprintf(num, sizeof(num), "%u",
			         (unsigned)(gEeprom.ScreenChannel[vfo] + 1u));
			const uint8_t num_w = smallest_width(num);
			if (num_w + 2u < name_left)
				GUI_DisplaySmallest(num, (uint8_t)(name_left - 2u - num_w), top, false, true);
		}
	}

	draw_right_small(freq_str, (uint8_t)(top + 8u));
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

/* Pack block b (0=bottom) into the full wave row so max height reaches SH_WAVE_Y */
static void rx_block_ys(uint8_t b_from_bottom, uint8_t *y0, uint8_t *y1)
{
	const uint8_t bot = (uint8_t)(SH_WAVE_Y + SH_WAVE_H - 1u);
	const uint16_t lo = ((uint16_t)b_from_bottom * SH_WAVE_H) / SH_RX_MAX_BLOCKS;
	const uint16_t hi = ((uint16_t)(b_from_bottom + 1u) * SH_WAVE_H) / SH_RX_MAX_BLOCKS;

	*y1 = (uint8_t)(bot - lo);
	*y0 = (uint8_t)(bot - (hi - 1u));
}

/* Top-center S / dBm — tight plate only; pillars still reach status-bar bottom */
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

	/* flush under status bar (framebuffer y=0); only a small center strip */
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
	GUI_DisplaySmallest(buf, text_x, text_y, false, true);
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

static void draw_wave_row(void)
{
	if (gCurrentFunction == FUNCTION_TRANSMIT) {
		draw_tx_wave();
	} else {
		/* RX or idle: keep pillars and let them decay — never hard-blank here */
		draw_rx_wave();
		draw_s_meter_label();
	}
}

static void blit_wave_lines(void)
{
	/* wave occupies y0..19 → framebuffer lines 0 and 1, plus top of line 2 */
	ST7565_BlitLine(0);
	ST7565_BlitLine(1);
	ST7565_BlitLine(2);
}

void UI_SyrupHome_Tick10ms(void)
{
	if (gScreenToDisplay != DISPLAY_MAIN)
		return;
	if (gLowBattery && !gLowBatteryConfirmed)
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
	if (tx && !s_was_tx)
		reset_wave_state();
	s_was_tx = tx;

	if (tx) {
		sample_tx_wave();
	} else if (rx) {
		sample_rx_wave();
	} else if (rx_pillars_alive()) {
		/* no signal / left RX — keep falling tips, do not clear */
		sample_rx_decay();
	} else {
		return;
	}

	draw_wave_row();
	blit_wave_lines();
}

void UI_DisplaySyrupHome(void)
{
	UI_DisplayClear();

	/* full-screen clear is fine on page entry; do not zero pillar state unless TX */
	if (gCurrentFunction == FUNCTION_TRANSMIT && !s_was_tx)
		reset_wave_state();

	draw_wave_row();
	draw_channel_row(0);
	draw_channel_row(1);
	invert_channel_row(gEeprom.TX_VFO);
}
