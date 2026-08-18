/*
 * Syrup Firmware
 *
 * Copyright (c) 2026 BD1AHN
 *
 * Licensed under the Apache License, Version 2.0
 */

#include "app/cw.h"
#include "keyboard_state.h"
#include "../driver/bk4819.h"
#include "../driver/gpio.h"
#include "../driver/backlight.h"
#include "../driver/st7565.h"
#include "../driver/system.h"
#include "../audio.h"
#include "../ui/helper.h"
#include <string.h>

#ifdef ENABLE_FEAT_F4HWN_GAME

static bool isInitialized;
static KeyboardState kbd;

#define CW_CHAR_BUFFER_SIZE 8
static char cwCharBuffer[CW_CHAR_BUFFER_SIZE];
static uint8_t cwCharIndex;

#define CW_TEXT_BUFFER_SIZE 48
static char cwTextBuffer[CW_TEXT_BUFFER_SIZE];
static uint8_t cwTextIndex;

// Auto-confirm timeout (15 loops * 40ms = 600ms)
#define AUTO_CONFIRM_TIMEOUT 15
static uint8_t idleCounter;

// Compact morse encoding: 3 bits length (1-5 -> 0-4) + 5 bits pattern (0=dot, 1=dash)
// Formula: encoded = ((len-1) << 5) | pattern
// Pattern: read morse from left to right, . = 0, - = 1
static const uint8_t morseEncode[36] = {
    // Letters A-Z (index 0-25)
    0x21, // A: .-
    0x68, // B: -...
    0x6A, // C: -.-.
    0x44, // D: -..
    0x00, // E: .
    0x62, // F: ..-.
    0x46, // G: --.
    0x60, // H: ....
    0x20, // I: ..
    0x67, // J: .---
    0x45, // K: -.-
    0x64, // L: .-..
    0x23, // M: --
    0x22, // N: -.
    0x47, // O: ---
    0x66, // P: .--.
    0x6D, // Q: --.-
    0x42, // R: .-.
    0x40, // S: ...
    0x01, // T: -
    0x41, // U: ..-
    0x61, // V: ...-
    0x43, // W: .--
    0x69, // X: -..-
    0x6B, // Y: -.--
    0x6C, // Z: --..
    // Numbers 0-9 (index 26-35)
    0x9F, // 0: -----
    0x8F, // 1: .----
    0x87, // 2: ..---
    0x83, // 3: ...--
    0x81, // 4: ....-
    0x80, // 5: .....
    0x90, // 6: -....
    0x98, // 7: --...
    0x9C, // 8: ---..
    0x9E  // 9: ----.
};

static char DecodeMorse(const char *code)
{
    uint8_t len = strlen(code);
    if (len < 1 || len > 5) return '?';

    uint8_t pattern = 0;
    for (uint8_t i = 0; i < len; i++) {
        pattern <<= 1;
        if (code[i] == '-') pattern |= 1;
    }

    uint8_t encoded = ((len - 1) << 5) | pattern;

    for (uint8_t i = 0; i < 36; i++) {
        if (morseEncode[i] == encoded) {
            if (i < 26) return 'A' + i;
            else return '0' + (i - 26);
        }
    }
    return '?';
}

static void ConfirmChar(void)
{
    if (cwCharIndex > 0) {
        char decoded = DecodeMorse(cwCharBuffer);
        if (cwTextIndex < CW_TEXT_BUFFER_SIZE - 1) {
            cwTextBuffer[cwTextIndex++] = decoded;
            cwTextBuffer[cwTextIndex] = '\0';
        }
        cwCharIndex = 0;
        cwCharBuffer[0] = '\0';
    }
    idleCounter = 0;
}

#define MAX_DISPLAY_CHARS 18

static void DrawCW(void)
{
    for (int row = 0; row < FRAME_LINES; row++) {
        memset(gFrameBuffer[row], 0, LCD_WIDTH);
    }
    memset(gStatusLine, 0, sizeof(gStatusLine));

    UI_PrintStringSmallBold("CW", 0, 127, 0);

    for (int x = 0; x < LCD_WIDTH; x++) {
        UI_DrawPixelBuffer(gFrameBuffer, (uint8_t)x, 8, true);
    }

    GUI_DisplaySmallest("menu:. exit:-", 0, 12, false, true);

    if (cwCharIndex > 0) {
        const char *s = cwCharBuffer;
        if (cwCharIndex > MAX_DISPLAY_CHARS)
            s += cwCharIndex - MAX_DISPLAY_CHARS;
        UI_PrintStringSmallNormal(s, 0, 0, 3);
    }

    if (cwTextIndex > 0) {
        const char *s = cwTextBuffer;
        if (cwTextIndex > MAX_DISPLAY_CHARS)
            s += cwTextIndex - MAX_DISPLAY_CHARS;
        UI_PrintStringSmallNormal(s, 0, 0, 5);
    }
}

static void AppendCW(char c)
{
    if (cwCharIndex < CW_CHAR_BUFFER_SIZE - 1) {
        cwCharBuffer[cwCharIndex++] = c;
        cwCharBuffer[cwCharIndex] = '\0';
    }
    idleCounter = 0;
}

static void PlayMorseTone(unsigned int duration_ms)
{
    BK4819_PlayTone(800, true);
    AUDIO_AudioPathOn();
    BK4819_ExitTxMute();
    SYSTEM_DelayMs(duration_ms);
    BK4819_EnterTxMute();
    AUDIO_AudioPathOff();
}

static bool HandleInput(void)
{
    kbd.prev = kbd.current;
    kbd.current = KEYBOARD_GetKey();

    if (kbd.current == KEY_EXIT) {
        kbd.counter++;
        if (kbd.counter > 20) {
            isInitialized = false;
            return false;
        }
    } else if (kbd.prev == KEY_EXIT && kbd.counter > 0 && kbd.counter <= 20) {
        AppendCW('-');
        PlayMorseTone(150);
        kbd.counter = 0;
    } else {
        kbd.counter = 0;
    }

    if (kbd.current == KEY_INVALID)
        return true;

    if (kbd.current != kbd.prev) {
        switch (kbd.current) {
        case KEY_MENU:
            AppendCW('.');
            PlayMorseTone(60);
            break;
        default:
            break;
        }
    }
    return true;
}

void APP_RunCW(void)
{
    BACKLIGHT_UpdateTickless();

    memset(cwCharBuffer, 0, CW_CHAR_BUFFER_SIZE);
    memset(cwTextBuffer, 0, CW_TEXT_BUFFER_SIZE);
    cwCharIndex = 0;
    cwTextIndex = 0;
    idleCounter = 0;
    isInitialized = true;
    kbd.current = KEY_INVALID;
    kbd.prev = KEY_INVALID;
    kbd.counter = 0;

    DrawCW();
    ST7565_BlitStatusLine();
    ST7565_BlitFullScreen();

    while (KEYBOARD_GetKey() != KEY_INVALID) {
        SYSTEM_DelayMs(10);
    }
    SYSTEM_DelayMs(100);

    while (isInitialized) {
        HandleInput();

        if (cwCharIndex > 0) {
            idleCounter++;
            if (idleCounter >= AUTO_CONFIRM_TIMEOUT) {
                ConfirmChar();
            }
        }

        DrawCW();
        ST7565_BlitStatusLine();
        ST7565_BlitFullScreen();
        SYSTEM_DelayMs(40);
    }
}

#endif
