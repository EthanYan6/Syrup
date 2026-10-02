#include "syrup_home.h"

#include <string.h>

#include "app/action.h"
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
#include "app/yan_id_rf.h"

/* Layout: two channel rows on top; S-meter band at bottom */
#define SH_STATUS_H        8u
#define SH_SCREEN_H        64u
#define SH_WAVE_H          20u
#define SH_CH_H            20u
#define SH_CH_GAP          2u
#define SH_CH0_Y           1u  /* both channel rows dropped 1px */
#define SH_CH1_Y           (SH_CH0_Y + SH_CH_H + SH_CH_GAP)     /* 23 */
#define SH_CH2_Y           44u /* former wave row — CH3 in triple watch */
#define SH_WAVE_Y          44u /* keep bottom band; 1px gap after CH1 */
#define SH_NAME_Y_OFF      0u
#define SH_PARAM_Y_OFF     8u  /* mod/pwr/sql — 4px above former +12 */
#define SH_TONE_Y_OFF      14u /* R:/T: subtones under params */
#define SH_FREQ_Y_OFF      12u /* right-column frequency (unchanged) */
#define SH_RIGHT_X_INSET   2u  /* right column shifted left */
#define SH_GAP_PX          2u

/* CH badge: solid black triangle attached to the inverted box's right edge */
#define SH_TAG_TRI_W       4u

/* S-meter band: left status column (lock/battery/text) + scale/line/needle */
#define SH_TICK_PERIOD_10MS  5u   /* ~50ms */
#define SH_METER_TEXT_H      6u   /* gFont3x5 glyph height */
#define SH_NEEDLE_MAX        128u /* 8 segments × 16 */
#define SH_NEEDLE_FALL       3u   /* steps toward 0 per tick when idle */
#define SH_METER_MARKS       9u
#define SH_SIDE_L_W          18u  /* left status column (battery icon is 17px) */
#define SH_METER_X0          (SH_SIDE_L_W + 3u)  /* 3px clear of the battery column */
#define SH_METER_X1          (LCD_WIDTH - 3u)
#define SH_NEEDLE_W          3u   /* slim vertical needle crossing the scale line */
#define SH_NEEDLE_UP         2u   /* needle rows above the scale line */
#define SH_NEEDLE_DN         3u   /* needle rows below the scale line */
#define SH_METER_LINE_Y      (SH_WAVE_Y + 8u)
#define SH_COL_LOCK_H        7u   /* gFontKeyLock visual height */
#define SH_COL_BAT_H         7u   /* battery icon visual height */
#define SH_COL_TEXT_H        6u   /* gFont3x5 text height */

/* Walkie: 7×7 body, 2×3 antenna on top-right (right edges aligned). */
#define SH_PHONE_W      7u
#define SH_PHONE_GAP    2u
#define SH_PHONE_ANT_W 2u
#define SH_PHONE_ANT_H 3u
#define SH_PHONE_BODY_H 7u

/* Speaking channel: speaker icon before the channel name */
#define SH_SPK_W        12u
#define SH_SPK_H        8u
#define SH_SPK_GAP      4u   /* gap between the icon and the name */
#define SH_SPK_Y_OFF    2u   /* icon drop vs the name row */

/* Channel names wider than 5 hanzi scroll inside a fixed right-anchored field
 * (Dondji dual-watch scheme): start → scroll left 1px/30ms → hold 2s at the
 * end → back to start → hold 2s → repeat. The seed resets when the text
 * changes, so every new name starts from its beginning. */
#define SH_NAME_HAN_CELL             13u  /* 12px glyph + 1px pitch */
#define SH_NAME_FIELD_HAN            5u
#define SH_NAME_FIELD_W              (SH_NAME_FIELD_HAN * SH_NAME_HAN_CELL - 1u)  /* 64px */
#define SH_NAME_FIELD_R              (LCD_WIDTH - SH_RIGHT_X_INSET - 1u)          /* 125 */
#define SH_NAME_FIELD_L              (SH_NAME_FIELD_R - SH_NAME_FIELD_W + 1u)     /* 62 */
#define SH_NAME_SCROLL_TICKS_PER_PX  3u   /* 1px / 30ms */
#define SH_NAME_PAUSE_TICKS          200u /* 2s @ 10ms */

/* CH badge triangle: solid black, attached to the box's right edge (bit0 = top) */
static const uint8_t s_tag_tri[SH_TAG_TRI_W] = {
	0b01111111, 0b00111110, 0b00011100, 0b00001000
};

/* Megaphone + sound waves (column-major, bit0 = top) */
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

static uint8_t  s_needle; /* 0 = S0 … SH_NEEDLE_MAX = +60 */
static int16_t  s_rx_dbm;
static uint32_t s_tx_wave_prng;
static bool     s_was_tx;
static bool     s_was_rx;
static uint8_t  s_triple_rx_vfo = 0xFF; /* sticky RX row in triple watch */
static uint16_t s_triple_rx_ch;
static uint32_t s_triple_rx_freq;
static uint8_t  s_triple_blink;
static uint8_t  s_tick_div;
static uint8_t  s_spk_vfo = 0xFF; /* last channel that received — sticky speaker */
static uint16_t s_spk_ch;
static uint32_t s_spk_freq;

static void draw_col_bitmap(uint8_t x, uint8_t y_top, const uint8_t *cols, uint8_t w);

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

static void draw_cjk_glyph_screen(uint16_t unicode, int16_t x, uint8_t y_top,
                                  int16_t clip_l, int16_t clip_r)
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
			const int16_t px = (int16_t)(x + (int16_t)col);
			if (px < clip_l || px > clip_r)
				continue;
			if (row_data & (uint16_t)(0x8000u >> col))
				draw_pixel((uint8_t)px, (uint8_t)(y_top + row), true);
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

/* x may be negative (scrolling); glyph columns outside [clip_l, clip_r] are skipped */
static void draw_small_text_ex(const char *text, int16_t x, uint8_t y_top, bool black,
                               uint8_t clip_l, uint8_t clip_r)
{
#ifdef ENABLE_CHINESE
	size_t i = 0;
	const bool mixed_cjk = SETTINGS_ChannelNameHasCjkUtf8(text);
	const uint8_t latin_y = mixed_cjk
		? UI_SmallLatinPixelY(y_top, (uint8_t)(y_top + 11u), true, 0u)
		: y_top;
	while (text[i] != '\0') {
		if (is_cjk_utf8(&text[i])) {
			(void)black;
			draw_cjk_glyph_screen(utf8_to_unicode(&text[i]), x, y_top,
			                      (int16_t)clip_l, (int16_t)clip_r);
			x = (int16_t)(x + 12u + 1u);
			i += 3u;
		} else {
			const char c = text[i];
			if (c > ' ' && c < 127) {
				const unsigned int index = (unsigned int)(c - ' ' - 1);
				const uint8_t *glyph = gFontSmall[index];
				const uint8_t char_w = (uint8_t)ARRAY_SIZE(gFontSmall[0]);
				const int16_t gx = (int16_t)(x + 1u);
				for (uint8_t col = 0; col < char_w; col++) {
					uint8_t bits = glyph[col];
					const int16_t px = (int16_t)(gx + (int16_t)col);
					if (px < (int16_t)clip_l || px > (int16_t)clip_r)
						continue;
					for (uint8_t row = 0; row < 8u; row++) {
						if (bits & (uint8_t)(1u << row))
							draw_pixel((uint8_t)px, (uint8_t)(latin_y + row), black);
					}
				}
			}
			x = (int16_t)(x + (uint16_t)ARRAY_SIZE(gFontSmall[0]) + 1u);
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
			const int16_t gx = (int16_t)(x + 1u);
			for (uint8_t col = 0; col < char_w; col++) {
				uint8_t bits = glyph[col];
				const int16_t px = (int16_t)(gx + (int16_t)col);
				if (px < (int16_t)clip_l || px > (int16_t)clip_r)
					continue;
				for (uint8_t row = 0; row < 8u; row++) {
					if (bits & (uint8_t)(1u << row))
						draw_pixel((uint8_t)px, (uint8_t)(y_top + row), black);
				}
			}
		}
		x = (int16_t)(x + pitch);
	}
	}
#endif
}

static void draw_small_text(const char *text, uint8_t x, uint8_t y_top, bool black)
{
	draw_small_text_ex(text, x, y_top, black, 0u, (uint8_t)(LCD_WIDTH - 1u));
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

static uint8_t channel_row_y(uint8_t vfo)
{
	if (vfo == 0u)
		return SH_CH0_Y;
	if (vfo == 1u)
		return SH_CH1_Y;
	return SH_CH2_Y;
}

static void invert_channel_row(uint8_t vfo)
{
	const uint8_t y0 = channel_row_y(vfo);
	const uint8_t y1 = (uint8_t)(y0 + SH_CH_H - 1u);

	/* Hairline above invert (CH1 uses the 1px left by dropping rows; row tops are always > 0) */
	{
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
	char buf[28];
	size_t dig_off = 0;
	const size_t dig_len = (digits != NULL) ? strlen(digits) : 0;
	const uint8_t x_left = 1u;
	uint8_t avail;

	if (x_right <= (uint8_t)(x_left + SH_GAP_PX))
		return;
	avail = (uint8_t)(x_right - SH_GAP_PX - x_left);

	while (dig_off < dig_len) {
		snprintf(buf, sizeof(buf), "DTMF:%s", digits + dig_off);
		if (smallest_width(buf) <= avail)
			break;
		dig_off++;
	}
	snprintf(buf, sizeof(buf), "DTMF:%s", digits + dig_off);
	draw_smallest_abs(buf, x_left, y, true);
}

static int16_t current_rssi_dbm(void)
{
	int16_t rssi_dBm = BK4819_GetRSSI_dBm();

#ifdef ENABLE_AM_FIX
	if (gSetting_AM_fix && gRxVfo->Modulation == MODULATION_AM)
		rssi_dBm = (int16_t)(rssi_dBm + AM_fix_get_gain_diff());
#endif
	{
		const unsigned int b = gEeprom.VfoInfo[gEeprom.RX_VFO].Band;
		if (b < 7u)
			rssi_dBm = (int16_t)(rssi_dBm + dBmCorrTable[b]);
	}
	return rssi_dBm;
}

static uint8_t current_s_level(void)
{
	const int16_t rssi_dBm = current_rssi_dbm();
	uint8_t s_level;

	/* IARU VHF/UHF: S9 = -93 dBm, 6 dB per S-unit */
	if (rssi_dBm >= -93)
		s_level = 9u;
	else if (rssi_dBm < -141)
		s_level = 0u;
	else
		s_level = (uint8_t)((rssi_dBm + 147) / 6);

	return s_level;
}

/* S0, S1, S3, S5, S7, S9, +20, +40, +60 (marks 2/4/6) */
static const int16_t s_meter_dbm[SH_METER_MARKS] = {
	-147, -141, -129, -117, -105, -93, -73, -53, -33
};

static uint8_t dbm_to_needle(int16_t dbm)
{
	uint8_t i;

	if (dbm <= s_meter_dbm[0])
		return 0u;
	if (dbm >= s_meter_dbm[SH_METER_MARKS - 1u])
		return SH_NEEDLE_MAX;

	for (i = 0u; i < (SH_METER_MARKS - 1u); i++) {
		const int16_t lo = s_meter_dbm[i];
		const int16_t hi = s_meter_dbm[i + 1u];
		if (dbm <= hi) {
			const int16_t span = (int16_t)(hi - lo);
			const int16_t frac = (int16_t)(dbm - lo);
			return (uint8_t)((uint16_t)i * 16u +
			                 (uint16_t)(frac * 16) / (uint16_t)span);
		}
	}
	return SH_NEEDLE_MAX;
}

static void remember_triple_rx_tune(void)
{
	if (s_triple_rx_vfo >= 3u)
		return;
	s_triple_rx_ch   = gEeprom.ScreenChannel[s_triple_rx_vfo];
	s_triple_rx_freq = gEeprom.VfoInfo[s_triple_rx_vfo].freq_config_RX.Frequency;
}

/* Speaker icon is sticky: latched on RX, stays on that channel row until
 * some other channel receives — or until that row is retuned with the
 * navigation keys (channel/memory or frequency changed). */
static void latch_spk_vfo(void)
{
	if (s_spk_vfo < 3u &&
	    (gEeprom.ScreenChannel[s_spk_vfo] != s_spk_ch ||
	     gEeprom.VfoInfo[s_spk_vfo].freq_config_RX.Frequency != s_spk_freq)) {
		s_spk_vfo = 0xFF;
	}

	if (FUNCTION_IsRx() && gEeprom.RX_VFO < 3u) {
		s_spk_vfo   = gEeprom.RX_VFO;
		s_spk_ch    = gEeprom.ScreenChannel[s_spk_vfo];
		s_spk_freq  = gEeprom.VfoInfo[s_spk_vfo].freq_config_RX.Frequency;
	}
}

static void latch_triple_rx(void)
{
	if (!gEeprom.TRIPLE_WATCH) {
		s_triple_rx_vfo = 0xFF;
		return;
	}

	if (FUNCTION_IsRx() && gEeprom.RX_VFO < 3u) {
		s_triple_rx_vfo = gEeprom.RX_VFO;
		remember_triple_rx_tune();
		return;
	}

	if (s_triple_rx_vfo >= 3u)
		return;

	/* Left/right (KEY_UP/DOWN) changed this row's memory or freq */
	if (gEeprom.ScreenChannel[s_triple_rx_vfo] != s_triple_rx_ch ||
	    gEeprom.VfoInfo[s_triple_rx_vfo].freq_config_RX.Frequency != s_triple_rx_freq)
		s_triple_rx_vfo = 0xFF;
}

static void draw_triple_rx_tag(uint8_t vfo)
{
	char sbuf[6];
	const uint8_t top = channel_row_y(vfo);
	const uint8_t badge_y = (uint8_t)(top + 1u);
	const uint8_t box_y0 = (uint8_t)(badge_y - 1u);
	const uint8_t box_y1 = (uint8_t)(badge_y + 5u);
	const uint8_t ch_w = smallest_width("CH1");
	const uint8_t ch_box_x1 = (uint8_t)(2u + ch_w);
	const uint8_t rx_w = smallest_width("RX");
	const uint8_t rx_x = (uint8_t)(ch_box_x1 + SH_TAG_TRI_W + 3u);
	const uint8_t rx_box_x0 = (uint8_t)(rx_x - 1u);
	const uint8_t rx_box_x1 = (uint8_t)(rx_x + rx_w);
	const uint8_t s_x = (uint8_t)(rx_box_x1 + 3u);
	const uint8_t s_w = smallest_width("S9");
	const bool live = FUNCTION_IsRx() && gEeprom.RX_VFO == vfo;

	if (!gEeprom.TRIPLE_WATCH || s_triple_rx_vfo != vfo)
		return;

	fill_rect(rx_box_x0, box_y0, (uint8_t)(s_x + s_w), box_y1, false);

	if ((gFlashLightBlinkCounter & 64u) != 0u) {
		fill_rect(rx_box_x0, box_y0, rx_box_x1, box_y1, true);
		draw_smallest_abs("RX", rx_x, badge_y, false);
	}

	if (live) {
		snprintf(sbuf, sizeof(sbuf), "S%u", (unsigned)current_s_level());
		draw_smallest_abs(sbuf, s_x, badge_y, true);
	}
}

/* --- channel-name scroll state (one slot per channel row) --- */
static uint16_t s_name_seed[3];       /* counter value when the text last changed */
static uint8_t  s_name_max_off[3];    /* name width − field width */
static uint8_t  s_name_drawn_off[3];  /* offset used by the last draw */
static bool     s_name_over[3];       /* this row's name is scrolling */
static char     s_name_text[3][CHANNEL_NAME_MAX_BYTES + 1u];

static void name_scroll_seed(uint8_t slot, const char *text)
{
	if (strncmp(s_name_text[slot], text, sizeof(s_name_text[0])) != 0) {
		strncpy(s_name_text[slot], text, sizeof(s_name_text[0]) - 1u);
		s_name_text[slot][sizeof(s_name_text[0]) - 1u] = '\0';
		s_name_seed[slot]      = gFlashLightBlinkCounter;
		s_name_drawn_off[slot] = 0u;
	}
}

static uint8_t name_scroll_offset(uint8_t slot)
{
	const uint16_t scroll_t = (uint16_t)(s_name_max_off[slot] * SH_NAME_SCROLL_TICKS_PER_PX);
	const uint16_t cycle    = (uint16_t)(scroll_t + 2u * SH_NAME_PAUSE_TICKS);
	const uint16_t t        = (uint16_t)((uint16_t)(gFlashLightBlinkCounter - s_name_seed[slot]) % cycle);

	if (t < scroll_t)
		return (uint8_t)(t / SH_NAME_SCROLL_TICKS_PER_PX);
	if (t < (uint16_t)(scroll_t + SH_NAME_PAUSE_TICKS))
		return s_name_max_off[slot];
	return 0u;
}

static void draw_channel_row(uint8_t vfo)
{
	const VFO_Info_t *info = &gEeprom.VfoInfo[vfo];
	const uint8_t top = channel_row_y(vfo);
	const uint8_t name_y = (uint8_t)(top + SH_NAME_Y_OFF);
	const uint8_t badge_y = (uint8_t)(top + 1u);
	const uint8_t param_y = (uint8_t)(top + SH_PARAM_Y_OFF);
	const uint8_t tone_y = (uint8_t)(top + SH_TONE_Y_OFF);
	const uint8_t freq_y = (uint8_t)(top + SH_FREQ_Y_OFF);
	const bool tx_vfo = (vfo == gEeprom.TX_VFO);
	const bool transmitting =
		(gCurrentFunction == FUNCTION_TRANSMIT && tx_vfo);
	const char *dtmf_digits = NULL; /* row 3: DTMF replaces the subtones */

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
		/* solid black triangle attached to the box's right edge */
		draw_col_bitmap((uint8_t)(box_x1 + 1u), box_y0, s_tag_tri, SH_TAG_TRI_W);
	}
	draw_triple_rx_tag(vfo);

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

	/* row 2: modulation, bandwidth, power, squelch */
	x = 1u;
	x = draw_param(gModulationStr[info->Modulation], x, param_y, true);
	if (info->Modulation == MODULATION_FM) {
		x = draw_param(info->CHANNEL_BANDWIDTH == BANDWIDTH_WIDE ? "W" : "N",
		               x, param_y, true);
	}
	x = draw_param(power_letter(info->OUTPUT_POWER), x, param_y, true);
	snprintf(String, sizeof(String), "%u", (unsigned)gEeprom.SQUELCH_LEVEL);
	draw_param(String, x, param_y, true);

	/* row 3: DTMF (keypad input on the TX row, or this channel's last
	 * received decode) takes the subtone row; width bound unchanged */
	if (gDTMF_InputMode && tx_vfo)
		dtmf_digits = gDTMF_InputBox;
	else if (gSetting_live_DTMF_decoder && gDTMF_RX_live[vfo][0] != 0)
		dtmf_digits = gDTMF_RX_live[vfo];

	if (dtmf_digits != NULL) {
		draw_dtmf_live(tone_y,
		               (uint8_t)(LCD_WIDTH - SH_RIGHT_X_INSET - small_text_width(freq_str)),
		               dtmf_digits);
	} else {
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
	{
		const bool show_yan =
			VfoState[vfo] == VFO_STATE_NORMAL &&
			gYanId_RX_timeout != 0 &&
			vfo == gYanId_RX_vfo;
		const bool rx_live =
			!show_yan && s_spk_vfo == vfo;
		uint8_t name_left;
		bool is_name = false;

		if (VfoState[vfo] != VFO_STATE_NORMAL &&
		    VfoState[vfo] < _VFO_STATE_LAST_ELEMENT &&
		    VfoStateStr[VfoState[vfo]] != NULL &&
		    VfoStateStr[VfoState[vfo]][0] != '\0') {
			snprintf(String, sizeof(String), "%s", VfoStateStr[VfoState[vfo]]);
		} else if (show_yan) {
			strncpy(String, gYanId_RX, sizeof(String) - 1u);
			String[sizeof(String) - 1u] = 0;
		} else {
			is_name = true;
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
			const uint8_t slot    = (vfo < 3u) ? vfo : 2u;
			const uint8_t name_dy = (uint8_t)(name_y + (show_yan ? SH_PHONE_ANT_H : 0u));
			const uint8_t name_w  = small_text_width(String);

			s_name_over[slot] = false;
			if (is_name && name_w > SH_NAME_FIELD_W) {
				/* name wider than the 5-hanzi field: scroll inside it */
				const uint8_t max_off = (uint8_t)(name_w - SH_NAME_FIELD_W);
				uint8_t off;

				name_scroll_seed(slot, String);
				s_name_max_off[slot] = max_off;
				off = name_scroll_offset(slot);
				s_name_over[slot]      = true;
				s_name_drawn_off[slot] = off;
				draw_small_text_ex(String,
				                   (int16_t)((int16_t)SH_NAME_FIELD_L - (int16_t)off),
				                   name_dy, true,
				                   (uint8_t)SH_NAME_FIELD_L, (uint8_t)SH_NAME_FIELD_R);
				name_left = (uint8_t)SH_NAME_FIELD_L;
			} else {
				name_left = draw_right_small(String, name_dy);
			}
		}
		if (show_yan) {
			const uint8_t ix = (uint8_t)(name_left - SH_PHONE_W - SH_PHONE_GAP);
			const uint8_t ax = (uint8_t)(ix + SH_PHONE_W - SH_PHONE_ANT_W);
			name_left = ix;
			fill_rect(ax, name_y,
			          (uint8_t)(ax + SH_PHONE_ANT_W - 1u),
			          (uint8_t)(name_y + SH_PHONE_ANT_H - 1u), true);
			fill_rect(ix, (uint8_t)(name_y + SH_PHONE_ANT_H),
			          (uint8_t)(ix + SH_PHONE_W - 1u),
			          (uint8_t)(name_y + SH_PHONE_ANT_H + SH_PHONE_BODY_H - 1u),
			          true);
		} else if (rx_live) {
			/* speaking on this channel: speaker icon sits left of the
			 * channel number, which keeps its usual spot by the name */
			uint8_t left_of_name = 0u; /* channel number + its gap */

			if (IS_MR_CHANNEL(gEeprom.ScreenChannel[vfo])) {
				char num[6];
				snprintf(num, sizeof(num), "%u",
				         (unsigned)(gEeprom.ScreenChannel[vfo] + 1u));
				const uint8_t num_w = smallest_width(num);
				if (num_w + 2u < name_left) {
					draw_smallest_abs(num,
						(uint8_t)(name_left - 2u - num_w), name_y, true);
					left_of_name = (uint8_t)(num_w + 2u);
				}
			}
			{
				const uint8_t gap = left_of_name ? 2u : SH_SPK_GAP;
				const uint8_t off = (uint8_t)(left_of_name + gap);
				if (name_left > off + SH_SPK_W)
					draw_col_bitmap((uint8_t)(name_left - off - SH_SPK_W),
					                (uint8_t)(name_y + SH_SPK_Y_OFF),
					                s_spk_bitmap, SH_SPK_W);
			}
		} else if (!show_yan && IS_MR_CHANNEL(gEeprom.ScreenChannel[vfo])) {
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

static void reset_wave_state(void)
{
	s_needle = 0u;
}

static bool needle_alive(void)
{
	return s_needle > 0u;
}

static void sample_needle(bool rx)
{
	if (rx) {
		s_rx_dbm = current_rssi_dbm();
		s_needle = dbm_to_needle(s_rx_dbm);
		return;
	}

	/* no signal: needle slowly walks back to 0 */
	if (s_needle > SH_NEEDLE_FALL)
		s_needle = (uint8_t)(s_needle - SH_NEEDLE_FALL);
	else
		s_needle = 0u;
}

static uint16_t tx_wave_rand_u16(void)
{
	uint32_t x = s_tx_wave_prng;
	if (x == 0u)
		x = 0xACE1u;
	x ^= x << 13;
	x ^= x >> 17;
	x ^= x << 5;
	s_tx_wave_prng = x;
	return (uint16_t)(x & 0xFFFFu);
}

static void clear_wave_area(void)
{
	const uint8_t y1 = (uint8_t)(SH_WAVE_Y + SH_WAVE_H - 1u);
	/* start one row above the band: also wipes the line over the 2/4/6 marks */
	fill_rect(0, (uint8_t)(SH_WAVE_Y - 1u), LCD_WIDTH - 1u, y1, false);
}

static bool wave_overlay_active(void)
{
	if (gLowBattery && !gLowBatteryConfirmed)
		return true;
	if (gEeprom.KEY_LOCK && gKeypadLocked > 0)
		return true;
	return false;
}

static const char *wave_prompt_text(void)
{
	if (gLowBattery && !gLowBatteryConfirmed) {
#ifdef ENABLE_CHINESE
		if (gUiLanguage == UI_LANGUAGE_CN)
			return "\xe4\xbd\x8e\xe7\x94\xb5\xe9\x87\x8f"; /* 低电量 */
#endif
		return "LOW BATTERY";
	}

	if (gEeprom.KEY_LOCK && gKeypadLocked > 0) {
#ifdef ENABLE_CHINESE
		if (gUiLanguage == UI_LANGUAGE_CN)
			return "\xe9\x95\xbf\xe6\x8c\x89#\xe8\xa7\xa3\xe9\x94\x81"; /* 长按#解锁 */
#endif
		return "UNLOCK KEYBOARD";
	}
	return NULL;
}

/* Non-uniform scale: marks 1..8 are evenly spaced and +60 lands on the right
 * edge; the 0→1 gap is exactly half of that spacing (S0..S1 spans only 6 dB).
 * The needle interpolates per segment, so it follows automatically. */
static uint8_t meter_tick_x(uint8_t i)
{
	const uint16_t span = (uint16_t)(SH_METER_X1 - SH_METER_X0);

	if (i == 0u)
		return SH_METER_X0;
	return (uint8_t)(SH_METER_X0 +
	                 (uint16_t)(((uint16_t)(2u * i - 1u) * span + 7u) / 15u));
}

static uint8_t needle_pixel_x(void)
{
	const uint8_t seg = (uint8_t)(s_needle / 16u);
	const uint8_t frac = (uint8_t)(s_needle % 16u);
	const uint8_t x0 = meter_tick_x(seg);

	if (seg >= (SH_METER_MARKS - 1u))
		return meter_tick_x(SH_METER_MARKS - 1u);
	{
		const uint8_t x1 = meter_tick_x((uint8_t)(seg + 1u));
		return (uint8_t)(x0 + ((uint16_t)(x1 - x0) * frac) / 16u);
	}
}

static void draw_scale_digit(char ch, uint8_t cx, uint8_t y, bool invert)
{
	char buf[2];
	const uint8_t gw = 3u;
	const uint8_t x = (uint8_t)(cx - gw / 2u); /* tick x ≥ 21 always */

	buf[0] = ch;
	buf[1] = '\0';
	if (invert) {
		const uint8_t bx0 = (uint8_t)(x - 1u);
		const uint8_t bx1 = (uint8_t)(x + gw);
		const uint8_t by0 = (uint8_t)(y - 1u); /* inverted blocks 1px taller */
		const uint8_t by1 = (uint8_t)(y + SH_METER_TEXT_H - 1u);
		fill_rect(bx0, by0, bx1, by1, true);
		draw_smallest_abs(buf, x, y, false);
	} else {
		draw_smallest_abs(buf, x, y, true);
	}
}

/* Scale + line + needle (decaying to 0 when idle) + live dBm readout */
static void draw_s_meter(bool rx)
{
	static const char marks[SH_METER_MARKS] = {
		'0', '1', '3', '5', '7', '9', '2', '4', '6'
	};
	const uint8_t num_y = SH_WAVE_Y;
	const uint8_t line_y = SH_METER_LINE_Y;
	const uint8_t x0 = meter_tick_x(0u);
	const uint8_t x1 = meter_tick_x(SH_METER_MARKS - 1u);
	uint8_t i;

	for (i = 0u; i < SH_METER_MARKS; i++) {
		const uint8_t tx = meter_tick_x(i);
		const bool inv = (i >= 6u); /* +20/+40/+60 marks inverted */
		draw_scale_digit(marks[i], tx, num_y, inv);
		draw_pixel(tx, (uint8_t)(line_y - 2u), true);
		draw_pixel(tx, (uint8_t)(line_y - 1u), true);
	}

	for (i = x0; i <= x1; i++)
		draw_pixel(i, line_y, true);

	/* slim vertical needle crossing the scale line at the signal position */
	{
		uint8_t nx = needle_pixel_x();
		uint8_t nx0 = (nx >= (SH_NEEDLE_W / 2u)) ? (uint8_t)(nx - SH_NEEDLE_W / 2u) : nx;
		uint8_t nx1;
		if (nx0 < x0)
			nx0 = x0;
		nx1 = (uint8_t)(nx0 + SH_NEEDLE_W - 1u);
		if (nx1 > x1)
			nx1 = x1;
		fill_rect(nx0, (uint8_t)(line_y - SH_NEEDLE_UP), nx1,
		          (uint8_t)(line_y + SH_NEEDLE_DN), true);
	}

	/* big dBm readout centered under the line while receiving */
	if (rx) {
		char buf[12];
		uint8_t tw;
		uint8_t tx;
		snprintf(buf, sizeof(buf), "%d dBm", (int)s_rx_dbm);
		tw = small_text_width(buf);
		if (tw >= (uint8_t)(x1 - x0))
			tx = x0;
		else
			tx = (uint8_t)(x0 + ((x1 - x0) - tw) / 2u);
		draw_small_text(buf, tx, (uint8_t)(line_y + SH_NEEDLE_DN + 1u), true);
	}
}

static void draw_tx_wave(void)
{
	const uint8_t wave_top = SH_WAVE_Y;
	const uint8_t wave_bottom = (uint8_t)(SH_WAVE_Y + SH_WAVE_H - 1u);
	const uint8_t inner_left = 0u;
	const uint8_t inner_right = (uint8_t)(LCD_WIDTH - 1u);
	const uint8_t center_y = (uint8_t)(((uint16_t)wave_top + (uint16_t)wave_bottom) / 2u);
	const uint8_t max_up = (uint8_t)(center_y - wave_top);
	const uint8_t max_dn = (uint8_t)(wave_bottom - center_y);
	const uint8_t n_cols = (uint8_t)(inner_right - inner_left + 1u);
	uint8_t peak = (uint8_t)((n_cols - 1u) / 2u);
	uint8_t col_idx;

	/* center axis + dense 1px columns, diamond envelope (Dondji TX popup) */
	for (uint8_t x = inner_left; x <= inner_right; x++)
		draw_pixel(x, center_y, true);

	if (peak == 0u)
		peak = 1u;

	col_idx = 0u;
	while (col_idx < n_cols) {
		uint16_t env_u16;
		uint8_t  up_limit;
		uint8_t  dn_limit;
		uint8_t  h_up;
		uint8_t  h_dn;
		uint16_t prng_a;
		uint16_t prng_b;

		if (n_cols <= 1u)
			env_u16 = 255u;
		else if (col_idx <= peak)
			env_u16 = (uint16_t)col_idx * 255u / (uint16_t)peak;
		else
			env_u16 = (uint16_t)(n_cols - 1u - col_idx) * 255u / (uint16_t)peak;

		up_limit = (uint8_t)(((uint16_t)max_up * env_u16) / 255u);
		dn_limit = (uint8_t)(((uint16_t)max_dn * env_u16) / 255u);

		prng_a = tx_wave_rand_u16();
		prng_b = tx_wave_rand_u16();

		h_up = (up_limit == 0u) ? 0u : (uint8_t)(prng_a % (uint16_t)(up_limit + 1u));
		h_dn = (dn_limit == 0u) ? 0u : (uint8_t)(prng_b % (uint16_t)(dn_limit + 1u));

		fill_rect((uint8_t)(inner_left + col_idx),
		          (uint8_t)(center_y - h_up),
		          (uint8_t)(inner_left + col_idx),
		          (uint8_t)(center_y + h_dn),
		          true);
		col_idx++;
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

/* Left status column: lock icon (when locked) / battery icon /
 * voltage-or-percent text (per gSetting_battery_text), stacked as rows. */
static void draw_left_status_column(void)
{
	char bat_str[8];
	uint8_t bat_bmp[sizeof(BITMAP_BatteryLevel1)];
	bool has_lock;
	bool has_text;
	bool spread;
	uint8_t row_h[3];
	uint8_t row_cnt = 0u;
	uint8_t total_h;
	uint8_t gap;
	uint8_t y;
	uint8_t i;
	uint8_t row = 0u;

	bat_str[0] = '\0';
	switch (gSetting_battery_text) {
	case 1: {
		const uint16_t voltage = MIN(gBatteryVoltageAverage, 999);
		snprintf(bat_str, sizeof(bat_str), "%u.%02u",
		         voltage / 100u, voltage % 100u);
		break;
	}
	case 2:
		snprintf(bat_str, sizeof(bat_str), "%02u%%",
		         BATTERY_VoltsToPercent(gBatteryVoltageAverage));
		break;
	default:
		break;
	}

	has_lock = (gEeprom.KEY_LOCK != 0);
	has_text = (bat_str[0] != '\0');

	if (has_lock)
		row_h[row_cnt++] = SH_COL_LOCK_H;
	row_h[row_cnt++] = SH_COL_BAT_H;
	if (has_text)
		row_h[row_cnt++] = SH_COL_TEXT_H;

	total_h = 0u;
	for (i = 0u; i < row_cnt; i++)
		total_h = (uint8_t)(total_h + row_h[i]);
	gap = (row_cnt > 1u) ? 1u : 0u;
	while ((uint16_t)total_h + (uint16_t)gap * (uint16_t)(row_cnt - 1u) > SH_WAVE_H && gap > 0u)
		gap--;
	total_h = (uint8_t)((uint16_t)total_h + (uint16_t)gap * (uint16_t)(row_cnt - 1u));

	y = (uint8_t)(SH_WAVE_Y + (SH_WAVE_H - total_h) / 2u);

	/* all three rows fill the band edge-to-edge: spread lock and text one
	 * extra pixel apart (gFont3x5 glyphs are 5px tall, so the text's empty
	 * 6th row clips harmlessly at the screen bottom) */
	spread = has_lock && has_text;

	if (has_lock) {
		const uint8_t x = (uint8_t)((SH_SIDE_L_W - sizeof(gFontKeyLock)) / 2u);
		const uint8_t ly = (spread && y > 0u) ? (uint8_t)(y - 1u) : y;
		draw_col_bitmap(x, ly, gFontKeyLock, (uint8_t)sizeof(gFontKeyLock));
		y = (uint8_t)(y + row_h[row] + gap);
		row++;
	}

	UI_DrawBattery(bat_bmp, gBatteryDisplayLevel, gLowBatteryBlink);
	draw_col_bitmap(0u, y, bat_bmp, (uint8_t)sizeof(BITMAP_BatteryLevel1));
	y = (uint8_t)(y + row_h[row] + gap);
	row++;

	if (has_text) {
		const uint8_t w = smallest_width(bat_str);
		const uint8_t x = (SH_SIDE_L_W > w) ? (uint8_t)((SH_SIDE_L_W - w) / 2u) : 0u;
		draw_smallest_abs(bat_str, x, (uint8_t)(y + (spread ? 1u : 0u)), true);
	}
	(void)row;
}

/* Prompt takes over the whole band (both watch modes): clear + centered text.
 * It disappears on its own timeout (keypad-lock counter / battery confirm),
 * then the normal band content is drawn again. */
static void draw_wave_overlay(void)
{
	const char *msg = wave_prompt_text();
	uint8_t text_h;
	uint8_t text_w;
	uint8_t x;
	uint8_t y;

	if (msg == NULL)
		return;

	clear_wave_area();

#ifdef ENABLE_CHINESE
	text_h = UI_SmallLinePixelHeight(msg);
#else
	text_h = 8u;
#endif
	if (text_h > SH_WAVE_H)
		text_h = SH_WAVE_H;

	text_w = small_text_width(msg);
	if (text_w >= LCD_WIDTH)
		x = 0u;
	else
		x = (uint8_t)((LCD_WIDTH - text_w) / 2u);
	y = (uint8_t)(SH_WAVE_Y + (SH_WAVE_H - text_h) / 2u);
	draw_small_text(msg, x, y, true);
}

/* TX: full-width TX animation only (no status column, no scale).
 * Prompt active: whole band cleared, centered text only, until it times out.
 * Otherwise: left status column + always-on S-meter scale/line/needle. */
static void draw_wave_row(void)
{
	clear_wave_area();

	if (gCurrentFunction == FUNCTION_TRANSMIT) {
		draw_tx_wave();
		return;
	}

	if (wave_overlay_active()) {
		draw_wave_overlay();
		return;
	}

	draw_left_status_column();
	draw_s_meter(FUNCTION_IsRx());
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

/* Scroll driver: request a full redraw whenever a scrolling name's offset
 * changed (the draw stores the offset it painted). CH3's row is skipped while
 * the prompt overlay covers it. */
static void name_scroll_tick(void)
{
	uint8_t rows = gEeprom.TRIPLE_WATCH ? 3u : 2u;

	if (rows > 2u && wave_overlay_active())
		rows = 2u;

	for (uint8_t s = 0u; s < rows; s++) {
		if (s_name_over[s] && name_scroll_offset(s) != s_name_drawn_off[s]) {
			gUpdateDisplay = true;
			return;
		}
	}
}

void UI_SyrupHome_Tick10ms(void)
{
	if (gScreenToDisplay != DISPLAY_MAIN)
		return;

	name_scroll_tick();

	if (++s_tick_div < SH_TICK_PERIOD_10MS)
		return;
	s_tick_div = 0;

	latch_spk_vfo();

	if (gEeprom.TRIPLE_WATCH) {
		const bool rx = FUNCTION_IsRx();
		latch_triple_rx();
		if (rx) {
			gUpdateDisplay = true;
		} else if (s_triple_rx_vfo < 3u) {
			const uint8_t blink = (uint8_t)((gFlashLightBlinkCounter >> 6) & 1u);
			if (s_was_rx || blink != s_triple_blink) {
				s_triple_blink = blink;
				gUpdateDisplay = true;
			}
		}
		s_was_rx = rx;
		return;
	}

	const bool tx = (gCurrentFunction == FUNCTION_TRANSMIT);
	const bool rx = FUNCTION_IsRx();

	if (tx && !GPIO_IsPttPressed()
#ifdef ENABLE_FEAT_F4HWN
	    && !ACTION_SidePttActive()
#endif
#ifdef ENABLE_VOX
	    && !gEeprom.VOX_SWITCH
#endif
	) {
		/* ignore spurious TX samples when PTT already released */
		return;
	}

	/* only entering TX wipes needle state */
	if (tx && !s_was_tx)
		reset_wave_state();
	s_was_tx = tx;

	if (!tx && rx) {
		sample_needle(true);
	} else if (!tx && needle_alive()) {
		/* signal gone: needle keeps drifting back toward 0 */
		sample_needle(false);
	}

	draw_wave_row();
	blit_wave_lines();
}

void UI_DisplaySyrupHome(void)
{
	UI_StatusClear();
	UI_DisplayClear();

	latch_spk_vfo();

	/* full-screen clear is fine on page entry; do not zero needle state unless TX */
	if (gCurrentFunction == FUNCTION_TRANSMIT && !s_was_tx)
		reset_wave_state();

	if (gEeprom.TRIPLE_WATCH) {
		latch_triple_rx();
		draw_channel_row(0);
		draw_channel_row(1);
		if (wave_overlay_active())
			draw_wave_overlay();
		else
			draw_channel_row(2);
		if (gEeprom.TX_VFO < 3u &&
		    !(wave_overlay_active() && gEeprom.TX_VFO == 2u))
			invert_channel_row(gEeprom.TX_VFO);
		return;
	}

	draw_wave_row();
	draw_channel_row(0);
	draw_channel_row(1);
	if (gEeprom.TX_VFO < 2u)
		invert_channel_row(gEeprom.TX_VFO);
}
