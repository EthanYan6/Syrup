/* Copyright 2026
 *
 * Licensed under the Apache License, Version 2.0.
 */

#ifndef APP_APRS_H
#define APP_APRS_H

#include <stdbool.h>
#include <stdint.h>

#include "driver/keyboard.h"

#ifdef ENABLE_APRS

#define APRS_CALLSIGN_MAX 9
#define APRS_COMMENT_MAX  81
#define APRS_ALT_UNK      ((int16_t)0x8000)

typedef struct {
	char     callsign[APRS_CALLSIGN_MAX + 1];
	uint16_t bearing_deg;  /* 0xFFFF = unknown */
	uint16_t distance_m;   /* 0xFFFF = unknown */
	uint16_t speed_kmh;    /* 0xFFFF = unknown */
	uint16_t course_deg;   /* 0xFFFF = unknown */
	uint16_t year;         /* 0 = unknown */
	uint8_t  month;
	uint8_t  day;
	int16_t  altitude_m;   /* APRS_ALT_UNK = unknown */
	char     comment[APRS_COMMENT_MAX];
	bool     has_target;
} Aprs_Target_t;

extern Aprs_Target_t gAprs;

void ACTION_Aprs(void);
void APRS_ProcessKeys(KEY_Code_t Key, bool bKeyPressed, bool bKeyHeld);
void UI_DisplayAprs(void);

void APRS_SetTarget(const char *callsign, uint16_t bearing_deg, uint16_t distance_m,
                    uint16_t speed_kmh, uint16_t course_deg,
                    uint16_t year, uint8_t month, uint8_t day, int16_t altitude_m,
                    const char *comment, uint8_t comment_len, bool open_page);

#endif /* ENABLE_APRS */

#endif
