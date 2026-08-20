/* Copyright 2026
 *
 * Licensed under the Apache License, Version 2.0.
 */

#ifndef APP_AIRCRAFT_H
#define APP_AIRCRAFT_H

#include <stdbool.h>
#include <stdint.h>

#include "driver/keyboard.h"

#ifdef ENABLE_AIRCRAFT_RADAR

#define AIRCRAFT_CALLSIGN_MAX 8
#define AIRCRAFT_DEFAULT_FREQ 12150000u /* 121.500 MHz, 10 Hz units */

typedef struct {
	char     callsign[AIRCRAFT_CALLSIGN_MAX + 1];
	int32_t  altitude_m;   /* INT32_MIN = unknown */
	uint16_t distance_m;   /* 0xFFFF = unknown; meters, saturates */
	uint32_t frequency;    /* 10 Hz units, airband AM */
	bool     has_target;
} Aircraft_Target_t;

extern Aircraft_Target_t gAircraft;

void ACTION_Aircraft(void);
void AIRCRAFT_ProcessKeys(KEY_Code_t Key, bool bKeyPressed, bool bKeyHeld);
void UI_DisplayAircraft(void);
void AIRCRAFT_Load(void);
void AIRCRAFT_Save(void);

/* Apply / re-apply AM receive on gAircraft.frequency */
void AIRCRAFT_Listen(void);

/* Update target fields (e.g. from UART). callsign may be NULL to leave unchanged. */
void AIRCRAFT_SetTarget(const char *callsign, int32_t altitude_m, uint16_t distance_m,
                        uint32_t frequency, bool listen_now, bool open_page);

#endif /* ENABLE_AIRCRAFT_RADAR */

#endif
