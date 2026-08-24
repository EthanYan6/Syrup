/* Copyright 2026
 *
 * Licensed under the Apache License, Version 2.0.
 *
 * APRS page: compass dart on the left, English fields on the right.
 * Nav keys (UP/DOWN) open the comment page (UTF-8 Chinese OK).
 */

#ifdef ENABLE_APRS

#include <stdint.h>
#include <string.h>

#include "app/aprs.h"
#include "app/generic.h"
#include "audio.h"
#include "driver/st7565.h"
#include "external/printf/printf.h"
#include "functions.h"
#include "misc.h"
#include "ui/helper.h"
#include "ui/inputbox.h"
#include "ui/status.h"
#include "ui/ui.h"

#define APRS_UNK 0xFFFFu
#define APRS_H   56
#define APRS_CX  25
#define APRS_R   21
#define APRS_LABEL_GAP 2u
#define APRS_LABEL_H   10u
/* ST7565 pixels read taller than wide: scale Y (×3/4) so the circle looks round. */
#define APRS_YM  3
#define APRS_YD  4
#define APRS_TX  50

/* BSS: empty page uses has_target=0 so fields show --- (0° is a valid bearing). */
Aprs_Target_t gAprs;
static uint8_t sPage;

/* sin(0..90°) step 6°, *255. icos = isin(d+90). */
static const uint8_t kSin[16] = {
	0, 27, 53, 79, 104, 128, 150, 171,
	191, 208, 225, 238, 248, 253, 255, 255
};

static int16_t isin(int16_t d)
{
	int16_t s = 1;

	if (d >= 360)
		d -= 360;
	if (d >= 180) {
		s = -1;
		d -= 180;
	}
	if (d > 90)
		d = 180 - d;
	return (int16_t)(s * kSin[d / 6]);
}

__attribute__((noinline))
static void plot(int x, int y)
{
	if ((unsigned)x < LCD_WIDTH && (unsigned)y < APRS_H)
		gFrameBuffer[y >> 3][x] |= (uint8_t)(1u << (y & 7));
}

/* Relative to compass centre; Y scaled so the circle looks round on this LCD. */
static int16_t sc_y_c(int16_t cy, int16_t dy)
{
	return (int16_t)(cy + (dy * APRS_YM) / APRS_YD);
}

/* Vertically centre circle + "APRS" label block in the left column. */
static int16_t compass_cy(void)
{
	const int16_t circle_h = sc_y_c(0, (int16_t)APRS_R) - sc_y_c(0, -(int16_t)APRS_R);
	const int16_t block_h = circle_h + (int16_t)APRS_LABEL_GAP + (int16_t)APRS_LABEL_H;

	return (int16_t)((APRS_H - block_h) / 2 - sc_y_c(0, -(int16_t)APRS_R));
}

static void plotc_c(int16_t cy, int dx, int dy)
{
	plot(APRS_CX + dx, sc_y_c(cy, dy));
}

static void circle_c(int16_t cy, int r)
{
	int x = r, y = 0, e = 1 - r;

	while (x >= y) {
		plotc_c(cy, +x, +y);
		plotc_c(cy, -x, +y);
		plotc_c(cy, +x, -y);
		plotc_c(cy, -x, -y);
		plotc_c(cy, +y, +x);
		plotc_c(cy, -y, +x);
		plotc_c(cy, +y, -x);
		plotc_c(cy, -y, -x);
		y++;
		if (e < 0)
			e += 2 * y + 1;
		else {
			x--;
			e += 2 * (y - x) + 1;
		}
	}
}

static void linec_c(int16_t cy, int x0, int y0, int x1, int y1)
{
	int dx = x1 - x0, dy = y1 - y0, sx = 1, sy = 1, err, e2;

	if (dx < 0) {
		dx = -dx;
		sx = -1;
	}
	if (dy < 0) {
		dy = -dy;
		sy = -1;
	}
	err = dx - dy;
	for (;;) {
		plotc_c(cy, x0, y0);
		if (x0 == x1 && y0 == y1)
			break;
		e2 = err << 1;
		if (e2 > -dy) {
			err -= dy;
			x0 += sx;
		}
		if (e2 < dx) {
			err += dx;
			y0 += sy;
		}
	}
}

static void rot(int8_t lx, int8_t ly, int16_t deg, int16_t *ox, int16_t *oy)
{
	const int16_t s = isin(deg);
	const int16_t c = isin((int16_t)(deg + 90));

	*ox = (int16_t)(((int32_t)lx * c - (int32_t)ly * s) >> 8);
	*oy = (int16_t)(((int32_t)lx * s + (int32_t)ly * c) >> 8);
}

/* Tip points to bearing: 0° = up (N), clockwise. */
static void dart_c(int16_t cy, uint16_t brg)
{
	int16_t tx, ty, lx, ly, nx, ny, rx, ry;
	int i;
	const int16_t deg = (brg == APRS_UNK) ? 0 : (int16_t)brg;

	rot(0, -18, deg, &tx, &ty);
	rot(-7, 11, deg, &lx, &ly);
	rot(0, 5, deg, &nx, &ny);
	rot(7, 11, deg, &rx, &ry);
	for (i = 0; i <= 8; i++) {
		linec_c(cy, tx, ty,
		        (int)((lx * (8 - i) + nx * i) >> 3),
		        (int)((ly * (8 - i) + ny * i) >> 3));
		linec_c(cy, tx, ty,
		        (int)((nx * (8 - i) + rx * i) >> 3),
		        (int)((ny * (8 - i) + ry * i) >> 3));
	}
}

void APRS_SetTarget(const char *callsign, uint16_t bearing_deg, uint16_t distance_m,
                    uint16_t speed_kmh, uint16_t course_deg, const char *comment,
                    uint8_t comment_len, bool open_page)
{
	uint8_t di = 0;

	gAprs.callsign[0] = '\0';
	if (callsign) {
		while (*callsign && di < APRS_CALLSIGN_MAX) {
			char c = *callsign++;
			if (c >= 'a' && c <= 'z')
				c = (char)(c - 'a' + 'A');
			if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-')
				gAprs.callsign[di++] = c;
			else if (c == ' ')
				break;
		}
		gAprs.callsign[di] = '\0';
	}

	gAprs.bearing_deg = (bearing_deg < 360u) ? bearing_deg : APRS_UNK;
	gAprs.distance_m  = distance_m;
	gAprs.speed_kmh   = speed_kmh;
	gAprs.course_deg  = (course_deg < 360u) ? course_deg : APRS_UNK;
	gAprs.has_target  = true;

	if (comment_len >= APRS_COMMENT_MAX)
		comment_len = APRS_COMMENT_MAX - 1u;
	if (!comment || !comment_len)
		gAprs.comment[0] = '\0';
	else {
		uint8_t i;
		memcpy(gAprs.comment, comment, comment_len);
		gAprs.comment[comment_len] = '\0';
		for (i = 0; gAprs.comment[i]; i++) {
			if (gAprs.comment[i] == '\r' || gAprs.comment[i] == '\n')
				gAprs.comment[i] = ' ';
		}
	}

	if (open_page) {
		sPage = 0;
		gUpdateStatus = true;
		gRequestDisplayScreen = DISPLAY_APRS;
		gUpdateDisplay = true;
	} else if (gScreenToDisplay == DISPLAY_APRS)
		gUpdateDisplay = true;
}

void ACTION_Aprs(void)
{
	if (gCurrentFunction == FUNCTION_TRANSMIT)
		return;

	gInputBoxIndex = 0;
	sPage = 0;
	gUpdateStatus = true;
	gRequestDisplayScreen = (gScreenToDisplay == DISPLAY_APRS) ? DISPLAY_MAIN : DISPLAY_APRS;
}

static void put_row(const char *s, uint8_t i)
{
	const uint8_t ys = (uint8_t)((i * APRS_H) / 5u);
	const uint8_t ye = (uint8_t)(((i + 1u) * APRS_H) / 5u - 1u);

	UI_PrintStringSmallAtPixel(s, APRS_TX, 0, ys, ye, 0u);
}

static void fmt_deg(char *d, const char *lab, uint16_t v)
{
	if (!gAprs.has_target || v == APRS_UNK)
		sprintf(d, "%s ---", lab);
	else
		sprintf(d, "%s %u", lab, v);
}

static void radar_page(void)
{
	char s[22];
	const char *cs = gAprs.callsign[0] ? gAprs.callsign : "--------";
	const int16_t cy = compass_cy();
	const uint8_t label_x0 = (uint8_t)(APRS_CX - APRS_R);
	const uint8_t label_x1 = (uint8_t)(APRS_CX + APRS_R);
	const uint8_t label_y0 = (uint8_t)(sc_y_c(cy, (int16_t)APRS_R) + (int16_t)APRS_LABEL_GAP);
	const uint8_t label_y1 = (uint8_t)(label_y0 + APRS_LABEL_H);

	circle_c(cy, APRS_R);
	circle_c(cy, APRS_R - 1);
	dart_c(cy, gAprs.bearing_deg);
	if (label_y1 < APRS_H)
		UI_PrintStringSmallAtPixel("APRS", label_x0, label_x1, label_y0, label_y1, 0u);

	sprintf(s, "CALL %s", cs);
	if (UI_SmallStringPixelWidth(s) > (LCD_WIDTH - APRS_TX))
		strcpy(s, cs);
	put_row(s, 0);

	fmt_deg(s, "BRG ", gAprs.bearing_deg);
	put_row(s, 1);

	if (!gAprs.has_target || gAprs.distance_m == APRS_UNK)
		strcpy(s, "DIST ---");
	else if (gAprs.distance_m < 10000u)
		sprintf(s, "DIST %u.%ukm", gAprs.distance_m / 1000u, (gAprs.distance_m % 1000u) / 100u);
	else
		sprintf(s, "DIST %ukm", gAprs.distance_m / 1000u);
	put_row(s, 2);

	if (!gAprs.has_target || gAprs.speed_kmh == APRS_UNK)
		strcpy(s, "SPD  ---");
	else if (gAprs.speed_kmh >= 1000u)
		strcpy(s, "SPD  999+");
	else
		sprintf(s, "SPD  %u", gAprs.speed_kmh);
	put_row(s, 3);

	fmt_deg(s, "CSE ", gAprs.course_deg);
	put_row(s, 4);
}

static void comment_page(void)
{
	const char *p = gAprs.comment[0] ? gAprs.comment : "---";
	char line[22];
	uint8_t row = 0, n = 0;

	line[0] = '\0';
	while (*p && row < 4u) {
		const uint8_t c = (uint8_t)*p;
		const uint8_t step = (c < 0x80u) ? 1u : ((c < 0xE0u) ? 2u : ((c < 0xF0u) ? 3u : 4u));
		uint8_t wrap = (*p == '\n') || (n + step >= 21u);

		if (!wrap && n) {
			memcpy(line + n, p, step);
			line[n + step] = '\0';
			wrap = UI_SmallStringPixelWidth(line) > 124u;
			line[n] = '\0';
		}
		if (wrap) {
			if (*p == '\n')
				p++;
			if (line[0]) {
				UI_PrintStringSmallAtPixel(line, 2, 0, (uint8_t)(4u + row * 12u),
				                           (uint8_t)(15u + row * 12u), 0u);
				row++;
				n = 0;
				line[0] = '\0';
				continue;
			}
			/* Empty line but still wrapping: skip one codepoint to avoid spin. */
			if (*p && *p != '\n')
				p += step;
			row++;
			continue;
		}
		memcpy(line + n, p, step);
		n = (uint8_t)(n + step);
		line[n] = '\0';
		p += step;
	}
	if (row < 4u && line[0])
		UI_PrintStringSmallAtPixel(line, 2, 0, (uint8_t)(4u + row * 12u),
		                           (uint8_t)(15u + row * 12u), 0u);
}

void UI_DisplayAprs(void)
{
	UI_DisplayClear();
	if (sPage)
		comment_page();
	else
		radar_page();
	UI_BlitFullScreen();
}

void APRS_ProcessKeys(KEY_Code_t Key, bool bKeyPressed, bool bKeyHeld)
{
	const bool tap = bKeyPressed && !bKeyHeld;

	if (Key == KEY_PTT) {
		GENERIC_Key_PTT(bKeyPressed);
		return;
	}
	if (!bKeyPressed && !bKeyHeld)
		return;

	if (Key == KEY_EXIT && tap) {
		if (sPage) {
			sPage = 0;
			gUpdateDisplay = true;
		} else {
			gRequestDisplayScreen = DISPLAY_MAIN;
			gUpdateStatus = true;
		}
		return;
	}
	if ((Key == KEY_UP || Key == KEY_DOWN) && tap) {
		sPage ^= 1u;
		gBeepToPlay = BEEP_1KHZ_60MS_OPTIONAL;
		gUpdateDisplay = true;
		return;
	}
	if (Key == KEY_F) {
		GENERIC_Key_F(bKeyPressed, bKeyHeld);
		return;
	}
	if (!bKeyHeld)
		gBeepToPlay = BEEP_500HZ_60MS_DOUBLE_BEEP_OPTIONAL;
}

#endif /* ENABLE_APRS */
