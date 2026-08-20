/* Copyright 2026
 *
 * Licensed under the Apache License, Version 2.0.
 *
 * Aircraft radar page: show callsign / altitude / distance / airband freq,
 * and tune BK4819 AM for listening (not broadcast FM).
 * Target is persisted in SPI flash so it survives USB unplug / reboot.
 */

#ifdef ENABLE_AIRCRAFT_RADAR

#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <string.h>

#include "app/aircraft.h"
#include "app/chFrScanner.h"
#include "app/common.h"
#include "app/generic.h"
#ifdef ENABLE_FMRADIO
#include "app/fm.h"
#endif
#include "audio.h"
#include "bitmaps.h"
#include "driver/py25q16.h"
#include "driver/st7565.h"
#include "external/printf/printf.h"
#include "font.h"
#include "frequencies.h"
#include "misc.h"
#include "radio.h"
#include "settings.h"
#include "ui/helper.h"
#include "ui/inputbox.h"
#include "ui/status.h"
#include "ui/ui.h"

/* VFO sector tail after foxhunt @ 0x0090E0..0x0090E6 (see eeprom_compat.c).
 * Must stay inside the 0x009000 mapping and below settings @ 0x00A000
 * (multi-system core/settings live map). */
#define AIRCRAFT_FLASH_ADDR  0x0090E7u
#define AIRCRAFT_FLASH_MAGIC 0xACu

typedef struct __attribute__((packed)) {
	uint8_t  magic;
	char     callsign[AIRCRAFT_CALLSIGN_MAX];
	int32_t  altitude_m;
	uint16_t distance_m;
	uint32_t frequency;
	uint8_t  flags; /* bit0 = has_target */
} Aircraft_Flash_t;

static_assert(sizeof(Aircraft_Flash_t) == 20);
static_assert(AIRCRAFT_FLASH_ADDR + sizeof(Aircraft_Flash_t) == 0x0090FBu);

Aircraft_Target_t gAircraft = {
	.callsign    = {0},
	.altitude_m  = INT32_MIN,
	.distance_m  = 0xFFFFu,
	.frequency   = AIRCRAFT_DEFAULT_FREQ,
	.has_target  = false,
};

#define AIRCRAFT_ICON_W 16

static uint32_t AIRCRAFT_ClampAirband(uint32_t freq)
{
	const uint32_t lo = frequencyBandTable[BAND2_108MHz].lower;
	const uint32_t hi = frequencyBandTable[BAND2_108MHz].upper - 1u;

	if (freq < lo)
		return lo;
	if (freq > hi)
		return hi;
	return freq;
}

static void AIRCRAFT_SanitizeCallsign(char *dst, const char *src)
{
	unsigned int di = 0;

	if (!src) {
		dst[0] = '\0';
		return;
	}

	for (unsigned int si = 0; src[si] && di < AIRCRAFT_CALLSIGN_MAX; si++) {
		char c = src[si];
		if (c >= 'a' && c <= 'z')
			c = (char)(c - 'a' + 'A');
		if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-')
			dst[di++] = c;
		else if (c == ' ')
			break;
	}
	dst[di] = '\0';
}

void AIRCRAFT_Save(void)
{
	Aircraft_Flash_t flash;
	Aircraft_Flash_t cur;

	memset(&flash, 0xFFu, sizeof(flash));
	flash.magic = AIRCRAFT_FLASH_MAGIC;
	memcpy(flash.callsign, gAircraft.callsign, AIRCRAFT_CALLSIGN_MAX);
	flash.altitude_m = gAircraft.altitude_m;
	flash.distance_m = gAircraft.distance_m;
	flash.frequency  = gAircraft.frequency;
	flash.flags      = gAircraft.has_target ? 1u : 0u;

	PY25Q16_ReadBuffer(AIRCRAFT_FLASH_ADDR, &cur, sizeof(cur));
	if (memcmp(&flash, &cur, sizeof(flash)) == 0)
		return;

	PY25Q16_WriteBuffer(AIRCRAFT_FLASH_ADDR, &flash, sizeof(flash), false);
}

void AIRCRAFT_Load(void)
{
	Aircraft_Flash_t flash;

	PY25Q16_ReadBuffer(AIRCRAFT_FLASH_ADDR, &flash, sizeof(flash));
	if (flash.magic != AIRCRAFT_FLASH_MAGIC)
		return;

	memcpy(gAircraft.callsign, flash.callsign, AIRCRAFT_CALLSIGN_MAX);
	gAircraft.callsign[AIRCRAFT_CALLSIGN_MAX] = '\0';
	/* Strip erased 0xFF padding */
	for (unsigned int i = 0; i < AIRCRAFT_CALLSIGN_MAX; i++) {
		if ((uint8_t)gAircraft.callsign[i] == 0xFFu)
			gAircraft.callsign[i] = '\0';
	}

	gAircraft.altitude_m = flash.altitude_m;
	gAircraft.distance_m = flash.distance_m;
	if (flash.frequency != 0 && flash.frequency != 0xFFFFFFFFu)
		gAircraft.frequency = AIRCRAFT_ClampAirband(flash.frequency);
	gAircraft.has_target = (flash.flags & 1u) != 0 && gAircraft.callsign[0] != '\0';
}

void AIRCRAFT_Listen(void)
{
	const unsigned int vfo = gEeprom.TX_VFO;
	VFO_Info_t *p = &gEeprom.VfoInfo[vfo];
	uint32_t freq = AIRCRAFT_ClampAirband(gAircraft.frequency);
	const FREQUENCY_Band_t band = BAND2_108MHz;

#ifdef ENABLE_FMRADIO
	if (gFmRadioMode)
		FM_TurnOff();
#endif

	gMonitor = false;

	if (gScanStateDir != SCAN_OFF) {
		gScanKeepResult = false;
		CHFRSCANNER_Stop();
	}

	if (p->Band != band ||
	    gEeprom.ScreenChannel[vfo] != (uint8_t)(FREQ_CHANNEL_FIRST + band)) {
		p->Band = band;
		gEeprom.ScreenChannel[vfo] = FREQ_CHANNEL_FIRST + band;
		gEeprom.FreqChannel[vfo]   = FREQ_CHANNEL_FIRST + band;
		RADIO_ConfigureChannel(vfo, VFO_CONFIGURE_RELOAD);
	}

	p->Modulation     = MODULATION_AM;
	p->STEP_SETTING  = STEP_25kHz;
	p->StepFrequency = gStepFrequencyTable[p->STEP_SETTING];
	freq = FREQUENCY_RoundToStep(freq, p->StepFrequency);
	freq = AIRCRAFT_ClampAirband(freq);

	p->freq_config_RX.Frequency = freq;
	p->freq_config_TX.Frequency = freq;
	p->freq_config_RX.CodeType  = CODE_TYPE_OFF;
	p->freq_config_TX.CodeType  = CODE_TYPE_OFF;
	p->TX_OFFSET_FREQUENCY_DIRECTION = TX_OFFSET_FREQUENCY_DIRECTION_OFF;
	p->FrequencyReverse = false;
	p->pRX = &p->freq_config_RX;
	p->pTX = &p->freq_config_TX;

	gAircraft.frequency = freq;

	RADIO_SelectVfos();
	RADIO_SetupRegisters(true);

	/* Do NOT set gRequestSaveChannel here: while a key is held,
	 * app.c forces gRequestDisplayScreen=MAIN and the aircraft page never sticks. */
	gUpdateStatus = true;
	AIRCRAFT_Save();
}

void AIRCRAFT_SetTarget(const char *callsign, int32_t altitude_m, uint16_t distance_m,
                       uint32_t frequency, bool listen_now, bool open_page)
{
	if (callsign)
		AIRCRAFT_SanitizeCallsign(gAircraft.callsign, callsign);

	gAircraft.altitude_m = altitude_m;
	gAircraft.distance_m = distance_m;
	if (frequency != 0)
		gAircraft.frequency = AIRCRAFT_ClampAirband(frequency);
	gAircraft.has_target = (gAircraft.callsign[0] != '\0');

	AIRCRAFT_Save();

	if (listen_now)
		AIRCRAFT_Listen();

	if (open_page) {
		gUpdateStatus = true;
		/* Prefer request so ProcessKey epilogue cannot stomp with MAIN */
		gRequestDisplayScreen = DISPLAY_AIRCRAFT;
		gUpdateDisplay = true;
	} else if (gScreenToDisplay == DISPLAY_AIRCRAFT) {
		gUpdateDisplay = true;
	}
}

void ACTION_Aircraft(void)
{
	if (gCurrentFunction == FUNCTION_TRANSMIT)
		return;

	gInputBoxIndex = 0;

	if (gScreenToDisplay == DISPLAY_AIRCRAFT) {
		AIRCRAFT_Save();
		gRequestSaveChannel = 1;
		gRequestDisplayScreen = DISPLAY_MAIN;
		gUpdateStatus = true;
		return;
	}

	AIRCRAFT_Listen();
	gUpdateStatus = true;
	gRequestDisplayScreen = DISPLAY_AIRCRAFT;
}

static void AIRCRAFT_StepFreq(int8_t dir)
{
	VFO_Info_t *p = gTxVfo;
	uint32_t step = p->StepFrequency ? p->StepFrequency : 2500u;
	uint32_t freq = gAircraft.frequency;

	if (dir > 0)
		freq += step;
	else if (freq > step)
		freq -= step;

	gAircraft.frequency = AIRCRAFT_ClampAirband(FREQUENCY_RoundToStep(freq, step));
	AIRCRAFT_Listen();
	gUpdateDisplay = true;
}

/* gInputBox: 0–9 = digit, 11 = decimal point (KEY_STAR while editing). */
#define AIRCRAFT_BOX_DOT 11
#define AIRCRAFT_FREQ_DIGITS 6

static void AIRCRAFT_ClearFreqEdit(void)
{
	gInputBoxIndex = 0;
	gKeyInputCountdown = 0;
}

static uint8_t AIRCRAFT_EditDigitCount(void)
{
	uint8_t n = 0;
	for (uint8_t i = 0; i < gInputBoxIndex; i++) {
		if (gInputBox[i] != AIRCRAFT_BOX_DOT && gInputBox[i] != 10)
			n++;
	}
	return n;
}

static int8_t AIRCRAFT_EditDotPos(void)
{
	for (uint8_t i = 0; i < gInputBoxIndex; i++) {
		if (gInputBox[i] == AIRCRAFT_BOX_DOT)
			return (int8_t)i;
	}
	return -1;
}

static uint8_t AIRCRAFT_EditFracDigits(void)
{
	const int8_t dot = AIRCRAFT_EditDotPos();
	uint8_t n = 0;

	if (dot < 0)
		return 0;
	for (uint8_t i = (uint8_t)(dot + 1); i < gInputBoxIndex; i++) {
		if (gInputBox[i] != 10)
			n++;
	}
	return n;
}

/* Build live preview: typed chars + '-' for remaining digit slots (max 6 digits). */
static void AIRCRAFT_FormatFreqEdit(char *dst, size_t dst_sz)
{
	size_t oi = 0;
	uint8_t digs = 0;

	for (uint8_t i = 0; i < gInputBoxIndex && oi + 1 < dst_sz; i++) {
		if (gInputBox[i] == AIRCRAFT_BOX_DOT) {
			dst[oi++] = '.';
		} else if (gInputBox[i] <= 9) {
			dst[oi++] = (char)('0' + gInputBox[i]);
			digs++;
		}
	}
	while (digs < AIRCRAFT_FREQ_DIGITS && oi + 1 < dst_sz) {
		dst[oi++] = '-';
		digs++;
	}
	dst[oi] = '\0';
}

static bool AIRCRAFT_CommitFreqEdit(void)
{
	uint8_t digits[AIRCRAFT_FREQ_DIGITS];
	uint8_t n = 0;
	int8_t  frac_start = -1;
	uint32_t freq;
	uint32_t step;

	for (uint8_t i = 0; i < gInputBoxIndex; i++) {
		if (gInputBox[i] == AIRCRAFT_BOX_DOT) {
			if (frac_start >= 0 || n == 0)
				return false;
			frac_start = (int8_t)n;
		} else if (gInputBox[i] <= 9) {
			if (n >= AIRCRAFT_FREQ_DIGITS)
				return false;
			digits[n++] = (uint8_t)gInputBox[i];
		}
	}

	if (n == 0)
		return false;

	if (frac_start < 0) {
		/* VFO-style: right-pad to 6 digits, then *100 → 10 Hz units */
		while (n < AIRCRAFT_FREQ_DIGITS)
			digits[n++] = 0;
		{
			uint32_t v = 0;
			for (uint8_t i = 0; i < AIRCRAFT_FREQ_DIGITS; i++)
				v = v * 10u + digits[i];
			freq = v * 100u;
		}
	} else {
		uint32_t ip = 0;
		uint32_t fp = 0;
		uint8_t  fi;

		if (frac_start == 0 || frac_start > 3)
			return false;

		for (uint8_t i = 0; i < (uint8_t)frac_start; i++)
			ip = ip * 10u + digits[i];

		fi = 0;
		for (uint8_t i = (uint8_t)frac_start; i < n && fi < 3; i++, fi++)
			fp = fp * 10u + digits[i];
		while (fi < 3) {
			fp *= 10u;
			fi++;
		}
		freq = ip * 100000u + fp * 100u;
	}

	step = gTxVfo->StepFrequency ? gTxVfo->StepFrequency : 2500u;
	gAircraft.frequency = AIRCRAFT_ClampAirband(FREQUENCY_RoundToStep(freq, step));
	AIRCRAFT_ClearFreqEdit();
	AIRCRAFT_Listen();
	gUpdateDisplay = true;
	return true;
}

static void AIRCRAFT_KeyDigit(KEY_Code_t Key)
{
	const int8_t dot = AIRCRAFT_EditDotPos();

	if (AIRCRAFT_EditDigitCount() >= AIRCRAFT_FREQ_DIGITS)
		return;
	if (dot >= 0 && AIRCRAFT_EditFracDigits() >= 3)
		return;

	INPUTBOX_Append(Key);
	gKeyInputCountdown = key_input_timeout_500ms;
	gBeepToPlay = BEEP_1KHZ_60MS_OPTIONAL;
	gUpdateDisplay = true;

	if (AIRCRAFT_EditDigitCount() >= AIRCRAFT_FREQ_DIGITS ||
	    (AIRCRAFT_EditDotPos() >= 0 && AIRCRAFT_EditFracDigits() >= 3)) {
		if (!AIRCRAFT_CommitFreqEdit()) {
			AIRCRAFT_ClearFreqEdit();
			gBeepToPlay = BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL;
			gUpdateDisplay = true;
		}
	}
}

static void AIRCRAFT_KeyStar(void)
{
	if (gInputBoxIndex == 0) {
		/* Idle: keep default-frequency shortcut */
		gAircraft.frequency = AIRCRAFT_DEFAULT_FREQ;
		AIRCRAFT_Listen();
		gUpdateDisplay = true;
		return;
	}

	if (AIRCRAFT_EditDotPos() >= 0) {
		gBeepToPlay = BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL;
		return;
	}
	/* Decimal only after 1–3 integer digits */
	if (AIRCRAFT_EditDigitCount() < 1 || AIRCRAFT_EditDigitCount() > 3) {
		gBeepToPlay = BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL;
		return;
	}
	if (gInputBoxIndex >= sizeof(gInputBox))
		return;

	gInputBox[gInputBoxIndex++] = AIRCRAFT_BOX_DOT;
	gKeyInputCountdown = key_input_timeout_500ms;
	gBeepToPlay = BEEP_1KHZ_60MS_OPTIONAL;
	gUpdateDisplay = true;
}

static void AIRCRAFT_DrawPixelXor(uint8_t x, uint8_t y)
{
	if (x >= LCD_WIDTH || y >= 56)
		return;
	gFrameBuffer[y >> 3][x] ^= (uint8_t)(1u << (y & 7u));
}

static void AIRCRAFT_DrawPixelSet(uint8_t x, uint8_t y)
{
	if (x >= LCD_WIDTH || y >= 56)
		return;
	gFrameBuffer[y >> 3][x] |= (uint8_t)(1u << (y & 7u));
}

/* 16px-tall column bitmap (page0 = y0..7, page1 = y8..15), top-left at (x, y). */
static void AIRCRAFT_Blit16At(uint8_t x, uint8_t y, const uint8_t *page0, const uint8_t *page1, uint8_t w)
{
	for (uint8_t col = 0; col < w; col++) {
		const uint16_t bits = (uint16_t)page0[col] | ((uint16_t)page1[col] << 8);
		for (uint8_t dy = 0; dy < 16; dy++) {
			if (bits & (1u << dy))
				AIRCRAFT_DrawPixelSet((uint8_t)(x + col), (uint8_t)(y + dy));
		}
	}
}

static void AIRCRAFT_PrintBigAt(const char *s, uint8_t x, uint8_t y, uint8_t char_pitch)
{
	for (; *s; s++) {
		if (*s > ' ' && *s < 127) {
			const unsigned int index = (unsigned int)(*s - ' ' - 1);
			const uint8_t *font = gFontBig[index];
			for (uint8_t col = 0; col < 7; col++) {
				const uint16_t bits = (uint16_t)font[col] | ((uint16_t)font[col + 7] << 8);
				for (uint8_t dy = 0; dy < 16; dy++) {
					if (bits & (1u << dy))
						AIRCRAFT_DrawPixelSet((uint8_t)(x + col), (uint8_t)(y + dy));
				}
			}
		}
		x = (uint8_t)(x + char_pitch);
	}
}

static void AIRCRAFT_InvertPixelRect(uint8_t x0, uint8_t x1, uint8_t y0, uint8_t y1)
{
	if (x1 > LCD_WIDTH)
		x1 = LCD_WIDTH;
	if (y1 > 55)
		y1 = 55;
	for (uint8_t y = y0; y <= y1; y++) {
		for (uint8_t x = x0; x < x1; x++)
			AIRCRAFT_DrawPixelXor(x, y);
	}
}

/* Tight vertical ink bounds in [y_lo, y_hi] across columns [x0, x1). */
static bool AIRCRAFT_FindInkY(uint8_t x0, uint8_t x1, uint8_t y_lo, uint8_t y_hi,
                             uint8_t *ymin, uint8_t *ymax)
{
	bool found = false;
	uint8_t lo = 255;
	uint8_t hi = 0;

	if (x1 > LCD_WIDTH)
		x1 = LCD_WIDTH;
	if (y_hi > 55)
		y_hi = 55;

	for (uint8_t y = y_lo; y <= y_hi; y++) {
		const uint8_t mask = (uint8_t)(1u << (y & 7u));
		const uint8_t *row = gFrameBuffer[y >> 3];
		for (uint8_t x = x0; x < x1; x++) {
			if (row[x] & mask) {
				if (!found || y < lo)
					lo = y;
				if (!found || y > hi)
					hi = y;
				found = true;
				break;
			}
		}
	}

	if (!found)
		return false;
	*ymin = lo;
	*ymax = hi;
	return true;
}

void UI_DisplayAircraft(void)
{
	const char *cs;
#ifdef ENABLE_CHINESE
	const bool cn = (gUiLanguage == UI_LANGUAGE_CN);
#endif
	/* Content row shifted down 2px from the top of the framebuffer */
	const uint8_t row_y = 2;

	UI_DisplayClear();

	/* Airplane icon + inverted large callsign (tight 1px pad above/below ink) */
	AIRCRAFT_Blit16At(2, row_y, BITMAP_Aircraft[0], BITMAP_Aircraft[1], AIRCRAFT_ICON_W);

	cs = gAircraft.callsign[0] ? gAircraft.callsign : "--------";
	{
		const uint8_t pitch = 8;
		const size_t len = strlen(cs);
		uint8_t start = 22;
		uint8_t end = 127;
		uint8_t text_w = (uint8_t)(len * pitch);
		uint8_t x0;
		uint8_t x1;
		uint8_t ymin;
		uint8_t ymax;

		if (end > start && text_w < (end - start))
			start = (uint8_t)(start + ((end - start) - text_w) / 2);

		AIRCRAFT_PrintBigAt(cs, start, row_y, pitch);

		x0 = (uint8_t)(start > 1 ? start - 1 : 0);
		x1 = (uint8_t)(start + text_w + 1);
		if (x1 > LCD_WIDTH)
			x1 = LCD_WIDTH;

		if (AIRCRAFT_FindInkY(start, (uint8_t)(start + text_w), row_y, (uint8_t)(row_y + 15),
		                      &ymin, &ymax)) {
			const uint8_t iy0 = (ymin > 0) ? (uint8_t)(ymin - 1u) : 0u;
			const uint8_t iy1 = (ymax < 55) ? (uint8_t)(ymax + 1u) : 55u;
			AIRCRAFT_InvertPixelRect(x0, x1, iy0, iy1);
		} else {
			AIRCRAFT_InvertPixelRect(x0, x1, row_y, (uint8_t)(row_y + 15u));
		}
	}

	/* Alt / Dist / Freq: three evenly spaced rows, left-aligned, no invert */
	{
		char line0[28];
		char line1[28];
		char line2[28];
		const uint8_t area_top = 19u;
		const uint8_t area_bot = 55u;
		const uint8_t area_h = (uint8_t)(area_bot - area_top + 1u);
		const uint32_t f = gAircraft.frequency;
		uint8_t i;

#ifdef ENABLE_CHINESE
		if (cn) {
			if (gAircraft.altitude_m == INT32_MIN)
				strcpy(line0, "高度 ---");
			else
				sprintf(line0, "高度 %ldm", (long)gAircraft.altitude_m);

			if (gAircraft.distance_m == 0xFFFFu)
				strcpy(line1, "距离 ---");
			else if (gAircraft.distance_m < 10000u)
				sprintf(line1, "距离 %u.%ukm",
				        gAircraft.distance_m / 1000u,
				        (gAircraft.distance_m % 1000u) / 100u);
			else
				sprintf(line1, "距离 %ukm", gAircraft.distance_m / 1000u);

			sprintf(line2, "频率 %u.%03u AM",
			        f / 100000u, (f / 100u) % 1000u);
			if (gInputBoxIndex > 0) {
				char edit[12];
				AIRCRAFT_FormatFreqEdit(edit, sizeof(edit));
				sprintf(line2, "频率 %s AM", edit);
			}
		} else
#endif
		{
			if (gAircraft.altitude_m == INT32_MIN)
				strcpy(line0, "ALT  ---");
			else
				sprintf(line0, "ALT  %ldm", (long)gAircraft.altitude_m);

			if (gAircraft.distance_m == 0xFFFFu)
				strcpy(line1, "DIST ---");
			else if (gAircraft.distance_m < 10000u)
				sprintf(line1, "DIST %u.%ukm",
				        gAircraft.distance_m / 1000u,
				        (gAircraft.distance_m % 1000u) / 100u);
			else
				sprintf(line1, "DIST %ukm", gAircraft.distance_m / 1000u);

			sprintf(line2, "FREQ %u.%03u AM",
			        f / 100000u, (f / 100u) % 1000u);
			if (gInputBoxIndex > 0) {
				char edit[12];
				AIRCRAFT_FormatFreqEdit(edit, sizeof(edit));
				sprintf(line2, "FREQ %s AM", edit);
			}
		}

		{
			const char *const lines[3] = { line0, line1, line2 };

			for (i = 0; i < 3u; i++) {
				const uint8_t y_s = (uint8_t)(area_top + (uint8_t)((i * area_h) / 3u));
				const uint8_t y_e = (uint8_t)(area_top + (uint8_t)(((i + 1u) * area_h) / 3u) - 1u);
				/* x_end=0 → left align (no horizontal centering) */
				UI_PrintStringSmallAtPixel(lines[i], 2, 0, y_s, y_e, 0u);
			}
		}
	}

	UI_BlitFullScreen();
}

void AIRCRAFT_ProcessKeys(KEY_Code_t Key, bool bKeyPressed, bool bKeyHeld)
{
	const bool short_press = bKeyPressed && !bKeyHeld;

	if (Key == KEY_PTT) {
		GENERIC_Key_PTT(bKeyPressed);
		return;
	}

	if (!bKeyPressed && !bKeyHeld)
		return;

	switch (Key) {
	case KEY_0:
	case KEY_1:
	case KEY_2:
	case KEY_3:
	case KEY_4:
	case KEY_5:
	case KEY_6:
	case KEY_7:
	case KEY_8:
	case KEY_9:
		if (short_press)
			AIRCRAFT_KeyDigit(Key);
		break;

	case KEY_STAR:
		if (short_press)
			AIRCRAFT_KeyStar();
		break;

	case KEY_EXIT:
		if (!short_press)
			break;
		if (gInputBoxIndex > 0) {
			gInputBox[--gInputBoxIndex] = 10;
			if (gInputBoxIndex == 0)
				AIRCRAFT_ClearFreqEdit();
			else
				gKeyInputCountdown = key_input_timeout_500ms;
			gUpdateDisplay = true;
			break;
		}
		AIRCRAFT_Save();
		gRequestSaveChannel = 1;
		gRequestDisplayScreen = DISPLAY_MAIN;
		gUpdateStatus = true;
		break;

	case KEY_MENU:
		if (!short_press)
			break;
		AIRCRAFT_ClearFreqEdit();
		AIRCRAFT_Listen();
		gBeepToPlay = BEEP_1KHZ_60MS_OPTIONAL;
		gUpdateDisplay = true;
		break;

	case KEY_UP:
	case KEY_DOWN:
		{
			int8_t dir = (Key == KEY_UP) ? 1 : -1;
			/* SET_NAV=0: UV-K1 left/right layout — invert like MAIN/FM */
			if (!gEeprom.SET_NAV)
				dir = (int8_t)(-dir);
			AIRCRAFT_ClearFreqEdit();
			AIRCRAFT_StepFreq(dir);
		}
		break;

	case KEY_F:
		/* Long-press F: same keypad lock as main/FM (status-bar padlock only). */
		GENERIC_Key_F(bKeyPressed, bKeyHeld);
		break;

	default:
		if (!bKeyHeld)
			gBeepToPlay = BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL;
		break;
	}
}

#endif /* ENABLE_AIRCRAFT_RADAR */
