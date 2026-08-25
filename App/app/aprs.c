/* Copyright 2026
 *
 * Licensed under the Apache License, Version 2.0.
 *
 * APRS page: compass dart on the left, English fields on the right.
 * Nav keys (UP/DOWN) cycle pages: 1 fields, 2 CSE/ALT + comment, 3 extra comment.
 * SET_NAV=0 (UV-K1 LEFT/RIGHT) inverts direction like MAIN/FM/aircraft.
 */

#ifdef ENABLE_APRS

#include <stdint.h>
#include <string.h>

#include "app/aprs.h"
#include "app/generic.h"
#include "audio.h"
#include "driver/st7565.h"
#include "external/printf/printf.h"
#include "font.h"
#include "functions.h"
#include "misc.h"
#include "settings.h"
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
/* First page: right-column fields sit 3px lower than the 5-row grid. */
#define APRS_RIGHT_YOFF 3u

#define APRS_CMT_LINE 40
#define APRS_CMT_MAX  8
#define APRS_CMT_H    12u
#define APRS_CMT_GAP  1u
#define APRS_CMT_W    124u
/* 3x5 "1/3" in the bottom-left; comments stop one pixel above it. */
#define APRS_IND_H    6u

/* BSS: empty page uses has_target=0 so fields show --- (0° is a valid bearing). */
Aprs_Target_t gAprs;
static uint8_t sPage;
static char    sCmt[APRS_CMT_MAX][APRS_CMT_LINE];
static uint8_t sCmtN;

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
                    uint16_t speed_kmh, uint16_t course_deg,
                    uint16_t year, uint8_t month, uint8_t day, int16_t altitude_m,
                    const char *comment, uint8_t comment_len, bool open_page)
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
	gAprs.year        = year;
	gAprs.month       = month;
	gAprs.day         = day;
	gAprs.altitude_m  = altitude_m;
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

static void row_band(uint8_t i, uint8_t *ys, uint8_t *ye)
{
	uint8_t y0 = (uint8_t)((i * APRS_H) / 5u + APRS_RIGHT_YOFF);
	uint8_t y1 = (uint8_t)(((i + 1u) * APRS_H) / 5u - 1u + APRS_RIGHT_YOFF);

	if (y1 >= APRS_H)
		y1 = APRS_H - 1u;
	*ys = y0;
	*ye = y1;
}

static void invert_rect(uint8_t x0, uint8_t x1, uint8_t y0, uint8_t y1)
{
	uint8_t x, y;

	if (x1 > LCD_WIDTH)
		x1 = LCD_WIDTH;
	if (y1 >= APRS_H)
		y1 = APRS_H - 1u;
	for (y = y0; y <= y1; y++) {
		const uint8_t mask = (uint8_t)(1u << (y & 7));
		uint8_t *row = gFrameBuffer[y >> 3];

		for (x = x0; x < x1; x++)
			row[x] ^= mask;
	}
}

static void put_row(const char *s, uint8_t i)
{
	uint8_t ys, ye;

	row_band(i, &ys, &ye);
	UI_PrintStringSmallAtPixel(s, APRS_TX, 0, ys, ye, 0u);
}

/* Small ° (bit0 = glyph top), Latin font has no 0xB0. */
static void draw_degree(uint8_t x, uint8_t y)
{
	static const uint8_t g[3] = { 0x02, 0x05, 0x02 };
	uint8_t col;
	const uint8_t line = (uint8_t)(y / 8);
	const uint8_t bit_offset = (uint8_t)(y & 7);

	if (line >= FRAME_LINES)
		return;
	for (col = 0; col < 3u; col++) {
		if ((uint16_t)x + col >= LCD_WIDTH)
			break;
		gFrameBuffer[line][x + col] |= (uint8_t)(g[col] << bit_offset);
		if (bit_offset + 7u > 8u && (line + 1u) < FRAME_LINES)
			gFrameBuffer[line + 1u][x + col] |= (uint8_t)(g[col] >> (8u - bit_offset));
	}
}

static void put_deg_xy(const char *lab, uint16_t v, uint8_t x, uint8_t ys, uint8_t ye)
{
	char s[22];

	if (!gAprs.has_target || v == APRS_UNK)
		sprintf(s, "%s ---", lab);
	else
		sprintf(s, "%s %u", lab, v);
	UI_PrintStringSmallAtPixel(s, x, 0, ys, ye, 0u);
	if (gAprs.has_target && v != APRS_UNK) {
		const uint8_t dx = (uint8_t)(x + (uint8_t)UI_SmallStringPixelWidth(s) + 1u);
#ifdef ENABLE_CHINESE
		draw_degree(dx, UI_SmallLatinPixelY(ys, ye, false, 0u));
#else
		draw_degree(dx, ys);
#endif
	}
}

static void put_deg_row(const char *lab, uint16_t v, uint8_t i)
{
	uint8_t ys, ye;

	row_band(i, &ys, &ye);
	put_deg_xy(lab, v, APRS_TX, ys, ye);
}

/* DATE in the same small font as BRG/SPD; YYYY-MM-DD is too wide, so 3x5 digits. */
static void print_3x5(const char *s, uint8_t x, uint8_t y)
{
	while (*s) {
		uint8_t c = (uint8_t)*s++;
		uint8_t i, j;

		if (c < 0x20u || c > 0x7Fu)
			c = (uint8_t)'?';
		c = (uint8_t)(c - 0x20u);
		for (i = 0; i < 3u; i++) {
			uint8_t pixels = gFont3x5[c][i];

			for (j = 0; j < 6u; j++) {
				if ((pixels & 1u) && ((uint16_t)x + i) < LCD_WIDTH && ((uint16_t)y + j) < APRS_H)
					gFrameBuffer[(y + j) >> 3][x + i] |= (uint8_t)(1u << ((y + j) & 7));
				pixels >>= 1;
			}
		}
		if (x > LCD_WIDTH - 4u)
			break;
		x = (uint8_t)(x + 4u);
	}
}

static void put_date_row(void)
{
	uint8_t ys, ye, x, y;
	char val[12];

	row_band(4, &ys, &ye);
	UI_PrintStringSmallAtPixel("DATE", APRS_TX, 0, ys, ye, 0u);

	if (!gAprs.has_target || gAprs.year < 2000u || gAprs.month < 1u || gAprs.month > 12u
	    || gAprs.day < 1u || gAprs.day > 31u)
		strcpy(val, "---");
	else
		sprintf(val, "%04u-%02u-%02u", gAprs.year, gAprs.month, gAprs.day);

	x = (uint8_t)(APRS_TX + (uint8_t)UI_SmallStringPixelWidth("DATE") + 7u);
#ifdef ENABLE_CHINESE
	y = UI_SmallLatinPixelY(ys, ye, false, 0u);
#else
	y = ys;
#endif
	print_3x5(val, x, y);
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
	{
		uint8_t ys, ye, x0, x1;
		const uint8_t tw = (uint8_t)UI_SmallStringPixelWidth(s);

		row_band(0, &ys, &ye);
		x0 = (APRS_TX > 0u) ? (uint8_t)(APRS_TX - 1u) : 0u;
		x1 = (uint8_t)(APRS_TX + tw + 1u);
		invert_rect(x0, x1, ys, ye);
	}

	put_deg_row("BRG ", gAprs.bearing_deg, 1);

	if (!gAprs.has_target || gAprs.distance_m == APRS_UNK)
		strcpy(s, "DIST ---");
	else if (gAprs.distance_m < 10000u)
		sprintf(s, "DIST %u.%ukm", gAprs.distance_m / 1000u, (gAprs.distance_m % 1000u) / 100u);
	else
		sprintf(s, "DIST %ukm", gAprs.distance_m / 1000u);
	put_row(s, 2);

	if (!gAprs.has_target || gAprs.speed_kmh == APRS_UNK)
		strcpy(s, "SPD ---");
	else if (gAprs.speed_kmh >= 1000u)
		strcpy(s, "SPD 999+");
	else
		sprintf(s, "SPD %ukm/h", gAprs.speed_kmh);
	put_row(s, 3);

	put_date_row();
}

static uint8_t utf8_step(const char *p)
{
	const uint8_t c = (uint8_t)*p;

	return (c < 0x80u) ? 1u : ((c < 0xE0u) ? 2u : ((c < 0xF0u) ? 3u : 4u));
}

static uint8_t wrap_comment(char out[][APRS_CMT_LINE], uint8_t max_lines)
{
	const char *p = gAprs.comment[0] ? gAprs.comment : "---";
	char line[APRS_CMT_LINE];
	uint8_t n = 0, nrows = 0;

	line[0] = '\0';
	while (*p && nrows < max_lines) {
		const uint8_t step = utf8_step(p);
		uint8_t wrap = (*p == '\n') || ((uint8_t)(n + step) >= (APRS_CMT_LINE - 1u));

		if (!wrap && n) {
			memcpy(line + n, p, step);
			line[n + step] = '\0';
			wrap = UI_SmallStringPixelWidth(line) > APRS_CMT_W;
			line[n] = '\0';
		}
		if (wrap) {
			if (*p == '\n')
				p++;
			if (line[0]) {
				memcpy(out[nrows], line, (size_t)n + 1u);
				nrows++;
				n = 0;
				line[0] = '\0';
				continue;
			}
			if (*p && *p != '\n')
				p += step;
			continue;
		}
		memcpy(line + n, p, step);
		n = (uint8_t)(n + step);
		line[n] = '\0';
		p += step;
	}
	if (nrows < max_lines && line[0]) {
		memcpy(out[nrows], line, (size_t)n + 1u);
		nrows++;
	}
	if (!nrows) {
		strcpy(out[0], "---");
		nrows = 1;
	}
	return nrows;
}

static void comment_refresh(void)
{
	sCmtN = wrap_comment(sCmt, APRS_CMT_MAX);
}

static uint8_t comment_fit(uint8_t y0, uint8_t y1)
{
	uint16_t area;
	const uint8_t stride = (uint8_t)(APRS_CMT_H + APRS_CMT_GAP);

	if (y1 < y0)
		return 0;
	area = (uint16_t)y1 - (uint16_t)y0 + 1u;
	return (uint8_t)((area + APRS_CMT_GAP) / stride);
}

static uint8_t comment_p2_y0(void)
{
	uint8_t ys, ye;

	row_band(1, &ys, &ye);
	return (uint8_t)(ye + 1u);
}

static uint8_t comment_y1(void)
{
	return (uint8_t)(APRS_H - APRS_IND_H - 1u);
}

static uint8_t aprs_page_count(void)
{
	comment_refresh();
	return (sCmtN > comment_fit(comment_p2_y0(), comment_y1())) ? 3u : 2u;
}

static void draw_page_index(void)
{
	char s[8];
	const uint8_t np = aprs_page_count();
	uint8_t cur = (uint8_t)(sPage + 1u);

	if (!np)
		return;
	if (cur > np)
		cur = np;
	sprintf(s, "%u/%u", cur, np);
	print_3x5(s, 1u, (uint8_t)(APRS_H - APRS_IND_H));
}

static void draw_comment_block(uint8_t start, uint8_t n, uint8_t y0, uint8_t y1)
{
	uint8_t i, y = y0;
	const uint8_t fit = comment_fit(y0, y1);

	if (!n || y1 < y0)
		return;
	if (n > fit)
		n = fit;
	for (i = 0; i < n; i++) {
		uint8_t ye = (uint8_t)(y + APRS_CMT_H - 1u);

		if (ye > y1)
			ye = y1;
		if (sCmt[start + i][0])
			UI_PrintStringSmallAtPixel(sCmt[start + i], 2, 0, y, ye, 0u);
		y = (uint8_t)(y + APRS_CMT_H + APRS_CMT_GAP);
		if (y > y1)
			break;
	}
}

static void detail_page(void)
{
	char s[24];
	uint8_t ys, ye, take;

	comment_refresh();

	row_band(0, &ys, &ye);
	put_deg_xy("CSE ", gAprs.course_deg, 2, ys, ye);

	row_band(1, &ys, &ye);
	if (!gAprs.has_target || gAprs.altitude_m == APRS_ALT_UNK)
		strcpy(s, "ALT  ---");
	else
		sprintf(s, "ALT  %dm", (int)gAprs.altitude_m);
	UI_PrintStringSmallAtPixel(s, 2, 0, ys, ye, 0u);

	take = comment_fit((uint8_t)(ye + 1u), comment_y1());
	if (take > sCmtN)
		take = sCmtN;
	draw_comment_block(0, take, (uint8_t)(ye + 1u), comment_y1());
}

static void comment_page(void)
{
	const uint8_t p2 = comment_fit(comment_p2_y0(), comment_y1());

	comment_refresh();
	if (sCmtN > p2)
		draw_comment_block(p2, (uint8_t)(sCmtN - p2),
		                  APRS_RIGHT_YOFF, comment_y1());
}

void UI_DisplayAprs(void)
{
	UI_DisplayClear();
	if (sPage == 0u)
		radar_page();
	else if (sPage == 1u)
		detail_page();
	else
		comment_page();
	draw_page_index();
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
		const uint8_t np = aprs_page_count();
		int8_t dir = (Key == KEY_UP) ? 1 : -1;

		/* SET_NAV=0: UV-K1 left/right layout — invert like MAIN/FM/aircraft */
		if (!gEeprom.SET_NAV)
			dir = (int8_t)(-dir);

		if (sPage >= np)
			sPage = 0;
		if (dir > 0)
			sPage = (uint8_t)((sPage + 1u) % np);
		else
			sPage = (uint8_t)((sPage + np - 1u) % np);
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
