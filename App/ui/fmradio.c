/* Copyright 2023 Dual Tachyon
 * https://github.com/DualTachyon
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 *     Unless required by applicable law or agreed to in writing, software
 *     distributed under the License is distributed on an "AS IS" BASIS,
 *     WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *     See the License for the specific language governing permissions and
 *     limitations under the License.
 */

#ifdef ENABLE_FMRADIO

#include <string.h>

#include "app/fm.h"
#include "driver/bk1080.h"
#include "driver/bk1080-regs.h"
#include "driver/st7565.h"
#include "external/printf/printf.h"
#include "font.h"
#include "misc.h"
#include "settings.h"
#include "ui/fmradio.h"
#include "ui/helper.h"
#include "ui/inputbox.h"
#include "ui/status.h"
#include "ui/ui.h"

/* VFO row ends at content y=39; 2px gap → EQ at y 42..55 (14px) */
#define FM_EQ_Y            42u
#define FM_EQ_H            14u
#define FM_EQ_BLOCK_W      5u
#define FM_EQ_TIP_H        1u
#define FM_EQ_PITCH_X      6u
#define FM_EQ_MAX_BLOCKS   4u
#define FM_EQ_COLS         (LCD_WIDTH / FM_EQ_PITCH_X)
#define FM_EQ_PAUSE_GATE   18u
#define FM_EQ_TICK_PERIOD  5u   /* ~50ms */
#define FM_EQ_TIP_FALL     5u
#define FM_EQ_RSSI_WEAK    10u

static uint8_t s_eq_band[FM_EQ_COLS];
static uint8_t s_eq_tip[FM_EQ_COLS];
static uint8_t s_eq_phase;
static uint8_t s_eq_tip_div;
static uint8_t s_eq_tick_div;
static bool    s_eq_was_visible;

static bool fm_eq_listen_visible(void)
{
	if (gFM_ScanState != FM_SCAN_OFF)
		return false;
	if (gAskToSave || gAskToDelete)
		return false;
#ifdef ENABLE_FEAT_F4HWN
	/* unlock hint uses line 5 — same rows as EQ */
	if (gEeprom.KEY_LOCK && gKeypadLocked > 0)
		return false;
#endif
	return true;
}

static void fm_eq_reset(void)
{
	memset(s_eq_band, 0, sizeof(s_eq_band));
	memset(s_eq_tip, 0, sizeof(s_eq_tip));
	s_eq_phase = 0;
	s_eq_tip_div = 0;
}

static void fm_draw_pixel(uint8_t x, uint8_t y, bool black)
{
	if (x >= LCD_WIDTH || y >= (FRAME_LINES * 8u))
		return;
	UI_DrawPixelBuffer(gFrameBuffer, x, y, black);
}

static void fm_fill_rect(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1, bool black)
{
	for (uint8_t y = y0; y <= y1; y++) {
		for (uint8_t x = x0; x <= x1 && x < LCD_WIDTH; x++)
			fm_draw_pixel(x, y, black);
	}
}

static void fm_draw_smallest(const char *text, uint8_t x, uint8_t y)
{
	uint8_t c;
	while ((c = (uint8_t)*text++) != '\0') {
		if (c < 0x20u || c > 0x7Fu)
			continue;
		c = (uint8_t)(c - 0x20u);
		for (uint8_t i = 0; i < 3u; i++) {
			uint8_t pixels = gFont3x5[c][i];
			for (uint8_t j = 0; j < 6u; j++) {
				if (pixels & 1u)
					fm_draw_pixel((uint8_t)(x + i), (uint8_t)(y + j), true);
				pixels >>= 1;
			}
		}
		x = (uint8_t)(x + 4u);
	}
}

static void fm_draw_band_label(void)
{
	char String[16];
	uint8_t len;
	uint8_t x;

	sprintf(String, "%d%s-%dM",
		BK1080_GetFreqLoLimit(gEeprom.FM_Band) / 10,
		gEeprom.FM_Band == 0 ? ".5" : "",
		BK1080_GetFreqHiLimit(gEeprom.FM_Band) / 10);

	len = (uint8_t)strlen(String);
	x = (uint8_t)(LCD_WIDTH - (len * 4u));
	fm_draw_smallest(String, x, 3u); /* was y=1; +2px down */
}

static uint8_t fm_energy_to_blocks(uint16_t energy)
{
	static const uint16_t thresholds[] = { 20u, 40u, 70u, 110u };
	uint8_t level = 0;

	for (uint8_t i = 0; i < ARRAY_SIZE(thresholds) && i < FM_EQ_MAX_BLOCKS; i++) {
		if (energy >= thresholds[i])
			level = (uint8_t)(i + 1u);
	}
	if (level > FM_EQ_MAX_BLOCKS)
		level = FM_EQ_MAX_BLOCKS;
	return level;
}

static uint16_t fm_read_activity(void)
{
	const uint16_t status = BK1080_ReadRegister(BK1080_REG_10);
	const uint16_t reg07  = BK1080_ReadRegister(BK1080_REG_07);
	const uint8_t  rssi   = (uint8_t)BK1080_REG_10_GET_RSSI(status);
	const uint8_t  snr    = (uint8_t)BK1080_REG_07_GET_SNR(reg07);
	const bool     railed = (status & BK1080_REG_10_MASK_AFCRL) != BK1080_REG_10_AFCRL_NOT_RAILED;
	uint16_t dynamics;

	if (railed || rssi < FM_EQ_RSSI_WEAK)
		return 0u;

	dynamics = (uint16_t)rssi + (uint16_t)(snr << 3);
	if (dynamics > 255u)
		dynamics = 255u;
	return dynamics;
}

static void fm_eq_sample_decay(void)
{
	for (uint8_t i = 0; i < FM_EQ_COLS; i++) {
		if (s_eq_band[i] > s_eq_tip[i])
			s_eq_tip[i] = s_eq_band[i];
		s_eq_band[i] = 0u;
	}

	if (++s_eq_tip_div >= FM_EQ_TIP_FALL) {
		s_eq_tip_div = 0;
		for (uint8_t i = 0; i < FM_EQ_COLS; i++) {
			if (s_eq_tip[i] > 0u)
				s_eq_tip[i]--;
		}
	}
}

static void fm_eq_sample(void)
{
	const uint16_t dynamics = fm_read_activity();
	uint16_t activity = dynamics;
	const bool paused = (dynamics < FM_EQ_PAUSE_GATE);

	if (paused) {
		fm_eq_sample_decay();
		return;
	}

	s_eq_phase++;

	for (uint8_t col = 0; col < FM_EQ_COLS; col++) {
		const uint16_t h = (uint16_t)(
			((uint16_t)s_eq_phase * 37u) ^
			((uint16_t)col * 157u) ^
			((uint16_t)(s_eq_phase + col) * 13u));
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

		{
			const uint8_t want = fm_energy_to_blocks(target);
			if (want > s_eq_band[col])
				s_eq_band[col]++;
			else if (want < s_eq_band[col] && s_eq_band[col] > 0u)
				s_eq_band[col]--;

			if (s_eq_band[col] > s_eq_tip[col])
				s_eq_tip[col] = s_eq_band[col];
		}
	}

	if (++s_eq_tip_div >= FM_EQ_TIP_FALL) {
		s_eq_tip_div = 0;
		for (uint8_t i = 0; i < FM_EQ_COLS; i++) {
			if (s_eq_tip[i] > s_eq_band[i])
				s_eq_tip[i]--;
		}
	}
}

static void fm_eq_block_ys(uint8_t b_from_bottom, uint8_t *y0, uint8_t *y1)
{
	const uint8_t bot = (uint8_t)(FM_EQ_Y + FM_EQ_H - 1u);
	const uint16_t lo = ((uint16_t)b_from_bottom * FM_EQ_H) / FM_EQ_MAX_BLOCKS;
	const uint16_t hi = ((uint16_t)(b_from_bottom + 1u) * FM_EQ_H) / FM_EQ_MAX_BLOCKS;

	*y1 = (uint8_t)(bot - lo);
	*y0 = (uint8_t)(bot - (hi - 1u));
}

static void fm_eq_draw_tip(uint8_t x0, uint8_t tip_level)
{
	uint8_t slot_y0;
	uint8_t slot_y1;
	uint8_t tip_y1;

	if (tip_level == 0u || FM_EQ_TIP_H == 0u)
		return;

	fm_eq_block_ys((uint8_t)(tip_level - 1u), &slot_y0, &slot_y1);
	tip_y1 = (uint8_t)(slot_y0 + FM_EQ_TIP_H - 1u);
	if (tip_y1 > slot_y1)
		tip_y1 = slot_y1;

	fm_fill_rect(x0, slot_y0, (uint8_t)(x0 + FM_EQ_BLOCK_W - 1u), tip_y1, true);
}

static void fm_eq_clear(void)
{
	fm_fill_rect(0, FM_EQ_Y, LCD_WIDTH - 1u, (uint8_t)(FM_EQ_Y + FM_EQ_H - 1u), false);
}

static void fm_eq_draw(void)
{
	fm_eq_clear();

	for (uint8_t col = 0; col < FM_EQ_COLS; col++) {
		const uint8_t x0   = (uint8_t)(col * FM_EQ_PITCH_X);
		const uint8_t body = s_eq_band[col];
		const uint8_t tip  = s_eq_tip[col];

		if (body > 0u) {
			for (uint8_t b = 0; b < body; b++) {
				uint8_t y0;
				uint8_t y1;
				fm_eq_block_ys(b, &y0, &y1);
				fm_fill_rect(x0, y0, (uint8_t)(x0 + FM_EQ_BLOCK_W - 1u), y1, true);
			}
			if (tip > 0u)
				fm_eq_draw_tip(x0, tip);
			else
				fm_eq_draw_tip(x0, body);
		} else if (tip > 0u) {
			fm_eq_draw_tip(x0, tip);
		}
	}
}

static void fm_eq_blit(void)
{
	ST7565_BlitLine(5);
	ST7565_BlitLine(6);
}

void UI_FM_Tick10ms(void)
{
	const bool visible = (gScreenToDisplay == DISPLAY_FM) && fm_eq_listen_visible();

	if (!visible) {
		if (s_eq_was_visible) {
			fm_eq_reset();
			s_eq_was_visible = false;
			s_eq_tick_div = 0;
		}
		return;
	}

	if (!s_eq_was_visible) {
		fm_eq_reset();
		s_eq_was_visible = true;
		s_eq_tick_div = 0;
	}

	if (++s_eq_tick_div < FM_EQ_TICK_PERIOD)
		return;
	s_eq_tick_div = 0;

	fm_eq_sample();
	fm_eq_draw();
	fm_eq_blit();
}

void UI_DisplayFM(void)
{
	char String[16];
	char *pPrintStr = String;
	UI_DisplayClear();

#ifdef ENABLE_FEAT_F4HWN
	UI_DisplayUnlockKeyboard(5);
#endif

	UI_PrintString("FM", 2, 0, 0, 8);

	fm_draw_band_label();

	if (gAskToSave) {
		pPrintStr = "SAVE?";
	} else if (gAskToDelete) {
		pPrintStr = "DEL?";
	} else if (gFM_ScanState == FM_SCAN_OFF) {
		if (gEeprom.FM_IsMrMode) {
			sprintf(String, "MR(CH%02u)", gEeprom.FM_SelectedChannel + 1);
			pPrintStr = String;
		} else {
			pPrintStr = "VFO";
			for (unsigned int i = 0; i < FM_CHANNELS_MAX; i++) {
				if (gEeprom.FM_FrequencyPlaying == gFM_Channels[i]) {
					sprintf(String, "VFO(CH%02u)", i + 1);
					pPrintStr = String;
					break;
				}
			}
		}
	} else if (gFM_AutoScan) {
		sprintf(String, "A-SCAN(%u)", gFM_ChannelPosition);
		pPrintStr = String;
	} else {
		pPrintStr = "M-SCAN";
	}

	UI_PrintString(pPrintStr, 0, 127, 3, 10); // memory, vfo, scan

	if (gAskToSave || (gEeprom.FM_IsMrMode && gInputBoxIndex > 0)) {
		UI_GenerateChannelString(String, gFM_ChannelPosition);
	} else if (gAskToDelete) {
		sprintf(String, "CH-%02u", gEeprom.FM_SelectedChannel + 1);
	} else {
		if (gInputBoxIndex == 0) {
			sprintf(String, "%3d.%d", gEeprom.FM_FrequencyPlaying / 10, gEeprom.FM_FrequencyPlaying % 10);
		} else {
			const char * ascii = INPUTBOX_GetAscii();
			sprintf(String, "%.3s.%.1s", ascii, ascii + 3);
		}

		UI_DisplayFrequency(String, 36, 1, gInputBoxIndex == 0);  // frequency

		if (fm_eq_listen_visible()) {
			s_eq_was_visible = true;
			fm_eq_draw();
		} else {
			fm_eq_reset();
			s_eq_was_visible = false;
		}

		UI_BlitFullScreen();
		return;
	}

	UI_PrintString(String, 0, 127, 1, 10);

	fm_eq_reset();
	s_eq_was_visible = false;

	UI_BlitFullScreen();
}

#endif
