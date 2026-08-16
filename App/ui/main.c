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

#include <string.h>
#include <stdlib.h>  // abs()

#include "app/app.h"
#ifdef ENABLE_FEAT_F4HWN_ACTION_PICKER
    #include "app/action.h"
#endif
#include "app/dtmf.h"


#ifdef ENABLE_AM_FIX
    #include "am_fix.h"
#endif
#include "bitmaps.h"
#include "board.h"
#include "driver/bk4819.h"
#include "driver/st7565.h"
#include "external/printf/printf.h"
#include "functions.h"
#include "helper/battery.h"
#include "misc.h"
#include "radio.h"
#include "settings.h"
#include "ui/helper.h"
#include "ui/inputbox.h"
#include "ui/main.h"
#include "ui/ui.h"
#include "audio.h"
#include "menu.h"

#ifdef ENABLE_FEAT_F4HWN
    #include "driver/system.h"
#endif

#include "ui/syrup_home.h"

center_line_t center_line = CENTER_LINE_NONE;

#ifdef ENABLE_FEAT_F4HWN
    // static int8_t RxBlink;
    static int8_t RxBlinkLed = 0;
    static int8_t RxBlinkLedCounter;
    static int8_t RxLine;


    static bool isMainOnly()
    {
        return (gEeprom.DUAL_WATCH == DUAL_WATCH_OFF) && (gEeprom.CROSS_BAND_RX_TX == CROSS_BAND_OFF);
    }
#endif

const char *const VfoStateStr[] = {
       [VFO_STATE_NORMAL]="",
       [VFO_STATE_BUSY]="BUSY",
       [VFO_STATE_BAT_LOW]="BAT LOW",
       [VFO_STATE_TX_DISABLE]="TX DISABLE",
       [VFO_STATE_TIMEOUT]="TIMEOUT",
       [VFO_STATE_ALARM]="ALARM",
       [VFO_STATE_VOLTAGE_HIGH]="VOLT HIGH"
};

#ifdef ENABLE_FEAT_F4HWN_SCAN_PROGRESS
#define SCAN_LIST_NAME_HOLD_500MS       (2000u / 500u)

static uint8_t gScanListNameCountdown_500ms;

void UI_MAIN_NotifyScanProgressDataChanged(void)
{
    gUpdateStatus = true;
}

void UI_MAIN_NotifyScanListChanged(void)
{
    UI_MAIN_NotifyScanProgressDataChanged();
    gScanListNameCountdown_500ms = SCAN_LIST_NAME_HOLD_500MS;
    gUpdateDisplay = true;
}

bool UI_MAIN_ShouldHoldScanResume(void)
{
    return gScanListNameCountdown_500ms > 0 && IS_MR_CHANNEL(gNextMrChannel);
}
#endif

// ----------------------------------------

#ifndef ENABLE_RSSI_BAR
static void DrawSmallPowerBars(uint8_t *p, unsigned int level)
{
    if(level>6)
        level = 6;

    char bar = 0b00111110;

    for(uint8_t i = 0; i <= level; i++) {
        if(gSetting_set_gui) {
            bar = (0xff << (6-i)) & 0x7F;
        }
        memset(p + 2 + i*3, bar, 2);
    }
}
#endif
#if defined ENABLE_AUDIO_BAR || defined ENABLE_RSSI_BAR

static void DrawLevelBar(uint8_t xpos, uint8_t line, uint8_t level, uint8_t bars)
{
#ifndef ENABLE_FEAT_F4HWN
    const char hollowBar[] = {
        0b01111111,
        0b01000001,
        0b01000001,
        0b01111111
    };
#endif
    
    uint8_t *p_line = gFrameBuffer[line];
    level = MIN(level, bars);

    for(uint8_t i = 0; i < level; i++) {
#ifdef ENABLE_FEAT_F4HWN
        if(gSetting_set_met)
        {
            const char hollowBar[] = {
                0b01111111,
                0b01000001,
                0b01000001,
                0b01111111
            };

            if(i < bars - 4) {
                for(uint8_t j = 0; j < 4; j++)
                    p_line[xpos + i * 5 + j] = (~(0x7F >> (i + 1))) & 0x7F;
            }
            else {
                memcpy(p_line + (xpos + i * 5), &hollowBar, ARRAY_SIZE(hollowBar));
            }
        }
        else
        {
            const char hollowBar[] = {
                0b00111110,
                0b00100010,
                0b00100010,
                0b00111110
            };

            const char simpleBar[] = {
                0b00111110,
                0b00111110,
                0b00111110,
                0b00111110
            };

            if(i < bars - 4) {
                memcpy(p_line + (xpos + i * 5), &simpleBar, ARRAY_SIZE(simpleBar));
            }
            else {
                memcpy(p_line + (xpos + i * 5), &hollowBar, ARRAY_SIZE(hollowBar));
            }
        }
#else
        if(i < bars - 4) {
            for(uint8_t j = 0; j < 4; j++)
                p_line[xpos + i * 5 + j] = (~(0x7F >> (i+1))) & 0x7F;
        }
        else {
            memcpy(p_line + (xpos + i * 5), &hollowBar, ARRAY_SIZE(hollowBar));
        }
#endif
    }
}
#endif

#ifdef ENABLE_AUDIO_BAR
// Approximation of a logarithmic scale using integer arithmetic
static uint8_t log2_approx(unsigned int value) {
    uint8_t log = 0;
    while (value >>= 1) {
        log++;
    }
    return log;
}
#endif

#ifdef ENABLE_AUDIO_BAR

void UI_DisplayAudioBar(void)
{
    if (gSetting_mic_bar)
    {
        if(gLowBattery && !gLowBatteryConfirmed)
            return;

#ifdef ENABLE_FEAT_F4HWN
        RxBlinkLed = 0;
        RxBlinkLedCounter = 0;
        BK4819_ToggleGpioOut(BK4819_GPIO6_PIN2_GREEN, false);
        unsigned int line;
        if (isMainOnly())
        {
            line = 5;
        }
        else
        {
            line = 3;
        }
#else
        const unsigned int line = 3;
#endif

        if (gCurrentFunction != FUNCTION_TRANSMIT ||
            gScreenToDisplay != DISPLAY_MAIN
#ifdef ENABLE_DTMF_CALLING
            || gDTMF_CallState != DTMF_CALL_STATE_NONE
#endif
            )
        {
            return;  // screen is in use
        }

#if defined(ENABLE_ALARM) || defined(ENABLE_TX1750)
        if (gAlarmState != ALARM_STATE_OFF)
            return;
#endif
        static uint8_t barsOld = 0;
        const uint8_t thresold = 18; // arbitrary thresold
        //const uint8_t barsList[] = {0, 0, 0, 1, 2, 3, 4, 5, 6, 8, 10, 13, 16, 20, 25, 25};
        const uint8_t barsList[] = {0, 0, 0, 1, 2, 3, 5, 7, 9, 12, 15, 18, 21, 25, 25, 25};
        uint8_t logLevel;
        uint8_t bars;

        unsigned int voiceLevel  = BK4819_GetVoiceAmplitudeOut();  // 15:0

        voiceLevel = (voiceLevel >= thresold) ? (voiceLevel - thresold) : 0;
        logLevel = log2_approx(MIN(voiceLevel * 16, 32768u) + 1);
        bars = barsList[logLevel];
        barsOld = (barsOld - bars > 1) ? (barsOld - 1) : bars;

        uint8_t *p_line = gFrameBuffer[line];
        memset(p_line, 0, LCD_WIDTH);

        DrawLevelBar(2, line, barsOld, 25);

        if (gCurrentFunction == FUNCTION_TRANSMIT)
            ST7565_BlitFullScreen();
    }
}
#endif

#ifdef ENABLE_FEAT_F4HWN_AUDIO_SCOPE

#define SCOPE_SAMPLES        43   // number of columns (43 脳 3px = 128px wide)
#define SCOPE_NOISE_GATE     50u  // minimum range below which the display shows baseline
#define SCOPE_FLOOR_RISE     2u   // floor rise per frame (+100 units/s at 20ms/frame)
#define SCOPE_FLOOR_DROP_SHR 3u   // floor drop IIR shift: drop by (floor-min) >> N per frame (~160ms to halve)
#define SCOPE_VOLUME_MIN     200u // let's assume that the sound level in silence is 200

void UI_DisplayAudioScope(void)
{
    static uint16_t g_scope_buf[SCOPE_SAMPLES];
    static uint8_t  g_scope_write      = 0;
    static uint16_t g_scope_floor      = SCOPE_VOLUME_MIN;     // persistent floor: snaps down fast, rises slowly
    static uint8_t  g_scope_ready      = 0;                    // number of valid samples since TX entry

    // REG_64 (VoiceAmplitudeOut) is only meaningful in TX (mic input).
    // FM RX audio is frequency-encoded 鈥?no register gives the instantaneous waveform.

// ------------------------------ Sample audio amplitude ------------------------------

    static bool s_was_tx = false;

    if (gCurrentFunction != FUNCTION_TRANSMIT) {
        s_was_tx = false;
        return;
    }

    // This prevents a sudden spike on the bar caused by release the PTT button
    if (!GPIO_IsPttPressed()
#ifdef ENABLE_VOX
    && !gEeprom.VOX_SWITCH
#endif
#ifdef ENABLE_FEAT_F4HWN
    && !gSetting_set_ptt_session
#endif
    )
    return;

    if (!s_was_tx) {
        // TX entry: full reset so every new transmission starts from a clean state
        for (uint8_t i = 0; i < SCOPE_SAMPLES; i++) g_scope_buf[i] = SCOPE_VOLUME_MIN;
        g_scope_write      = 0u;
        g_scope_floor      = SCOPE_VOLUME_MIN;
        s_was_tx           = true;
    }

    // The first 7 bars after turning on the radio
    // will not display any values: they cause high bars.
    if (g_scope_ready >= 7)
        g_scope_buf[g_scope_write] = BK4819_GetVoiceAmplitudeOut();
    else
        g_scope_ready++;
        
    // If the reading is 0, it is definitely an incorrect value
    // caused by the microphone being muted - set it to 200.
    if (g_scope_buf[g_scope_write] == 0) 
        g_scope_buf[g_scope_write] =  SCOPE_VOLUME_MIN;

    g_scope_write = (g_scope_write + 1u) % SCOPE_SAMPLES;

// --------------------------------- Refresh display ---------------------------------

    if (gLowBattery && !gLowBatteryConfirmed)
        return;

    if (gScreenToDisplay != DISPLAY_MAIN
#ifdef ENABLE_DTMF_CALLING
        || gDTMF_CallState != DTMF_CALL_STATE_NONE
#endif
        )
        return;

#if defined(ENABLE_ALARM) || defined(ENABLE_TX1750)
    if (gAlarmState != ALARM_STATE_OFF)
        return;
#endif

#ifdef ENABLE_FEAT_F4HWN
    RxBlinkLed = 0;
    RxBlinkLedCounter = 0;
    BK4819_ToggleGpioOut(BK4819_GPIO6_PIN2_GREEN, false);
    const unsigned int line = isMainOnly() ? 5 : 3;
#else
    const unsigned int line = 3;
#endif

    uint8_t *p_line = gFrameBuffer[line];
    memset(p_line, 0, LCD_WIDTH);

    // Find min and max across current buffer
    uint16_t min_val = g_scope_buf[0];
    uint16_t max_val = g_scope_buf[0];
    for (uint8_t i = 1u; i < SCOPE_SAMPLES; i++) {
        if (g_scope_buf[i] < min_val) min_val = g_scope_buf[i];
        if (g_scope_buf[i] > max_val) max_val = g_scope_buf[i];
    }

    // Floor tracks buffer minimum with asymmetric IIR:
    // - drops toward min smoothly (SCOPE_FLOOR_DROP_SHR), avoiding instant-snap ghost
    // - rises slowly (SCOPE_FLOOR_RISE/frame) to handle loud constant voice
    if (g_scope_floor > min_val)
        g_scope_floor -= ((g_scope_floor - min_val) >> SCOPE_FLOOR_DROP_SHR) + 1u;
    else
        g_scope_floor += SCOPE_FLOOR_RISE;

    const uint16_t range = (max_val > g_scope_floor) ? (max_val - g_scope_floor) : 0u;

    for (uint8_t i = 0u; i < SCOPE_SAMPLES; i++) {
        const uint8_t  idx    = (g_scope_write + i) % SCOPE_SAMPLES;
        uint8_t        height = 0u;
        if (range >= SCOPE_NOISE_GATE) {
            const uint16_t v = (g_scope_buf[idx] > g_scope_floor) ? (g_scope_buf[idx] - g_scope_floor) : 0u;
            height = (uint8_t)((uint32_t)v * 7u / range);
        }
        // Filled column using bits 6..0 only (bit 7 always off to avoid overlap with text below)
        // At silence (height 0): single pixel at bit 6 (baseline)
        const uint8_t mask = (height > 0u) ? (uint8_t)((0x7Fu << (7u - height)) & 0x7Fu) : 0x40u;
        // 2px column + 1px gap per sample

        uint8_t *p_col = &p_line[i * 3u];
        p_col[0] = mask;
        p_col[1] = mask;

    }

    ST7565_BlitLine(line);
}
#endif  // ENABLE_FEAT_F4HWN_AUDIO_SCOPE

void DisplayRSSIBar(const bool now)
{
#if defined(ENABLE_RSSI_BAR)
    if (APP_IsScreenSaverDisplayed())
        return;

    const unsigned int txt_width    = 7 * 8;                 // 8 text chars
    const unsigned int bar_x        = 2 + txt_width + 4;     // X coord of bar graph

#ifdef ENABLE_FEAT_F4HWN
    /*
    const char empty[] = {
        0b00000000,
        0b00000000,
        0b00000000,
        0b00000000,
        0b00000000,
        0b00000000,
        0b00000000,
    };
    */

    unsigned int line;
    if (isMainOnly())
    {
        line = 5;
    }
    else
    {
        line = 3;
    }

    //char rx[4];
    //sprintf(String, "%d", RxBlink);
    //UI_PrintStringSmallBold(String, 80, 0, RxLine);

    if(RxLine >= 0 && center_line != CENTER_LINE_IN_USE)
    {
        static bool clean = false;
        uint8_t *p_line0 = gFrameBuffer[RxLine + 0];

        clean = !clean;

        if(clean) {
            for(uint8_t i = 0; i < sizeof(BITMAP_VFO_Default); i++)
                p_line0[i] = (p_line0[i] & 0x80) | BITMAP_VFO_Default[i];
        } else {
            for(uint8_t i = 0; i < sizeof(BITMAP_VFO_Empty); i++)
                p_line0[i] = (p_line0[i] & 0x80) | BITMAP_VFO_Empty[i];
        }

        ST7565_DrawLine(0, RxLine + 1, p_line0, sizeof(BITMAP_VFO_Default));
    }

#else
    const unsigned int line = 3;
#endif
    uint8_t           *p_line        = gFrameBuffer[line];
    char               str[16];
#ifdef ENABLE_FEAT_F4HWN
    uint8_t            oldLine[LCD_WIDTH];
#endif

#ifndef ENABLE_FEAT_F4HWN
    const char plus[] = {
        0b00011000,
        0b00011000,
        0b01111110,
        0b01111110,
        0b01111110,
        0b00011000,
        0b00011000,
    };
#endif

    if ((gEeprom.KEY_LOCK && gKeypadLocked > 0) || center_line != CENTER_LINE_RSSI)
        return;     // display is in use

    if (gCurrentFunction == FUNCTION_TRANSMIT ||
        gScreenToDisplay != DISPLAY_MAIN
#ifdef ENABLE_DTMF_CALLING
        || gDTMF_CallState != DTMF_CALL_STATE_NONE
#endif
        )
        return;     // display is in use

#ifdef ENABLE_FEAT_F4HWN
    if (now) {
        memcpy(oldLine, p_line, LCD_WIDTH);
        memset(p_line, 0, LCD_WIDTH);
    }
#else
    if (now)
        memset(p_line, 0, LCD_WIDTH);
#endif

#ifdef ENABLE_FEAT_F4HWN
    int16_t rssi_dBm =
        BK4819_GetRSSI_dBm()
#ifdef ENABLE_AM_FIX
        + ((gSetting_AM_fix && gRxVfo->Modulation == MODULATION_AM) ? AM_fix_get_gain_diff() : 0)
#endif
        + dBmCorrTable[gRxVfo->Band];

    // IARU VHF/UHF S-meter: S9 = -93 dBm, 1 S-unit = 6 dB
    // S(n) threshold = -93 + (n - 9) * 6
    uint8_t s_level    = 0;
    uint8_t overS9dBm  = 0;
    uint8_t overS9Bars = 0;

    // if      (rssi_dBm >= -93)  s_level = 9;  // S9  = -93 dBm
    // else if (rssi_dBm >= -99)  s_level = 8;  // S8  = -99 dBm
    // else if (rssi_dBm >= -105) s_level = 7;  // S7  = -105 dBm
    // else if (rssi_dBm >= -111) s_level = 6;  // S6  = -111 dBm
    // else if (rssi_dBm >= -117) s_level = 5;  // S5  = -117 dBm
    // else if (rssi_dBm >= -123) s_level = 4;  // S4  = -123 dBm
    // else if (rssi_dBm >= -129) s_level = 3;  // S3  = -129 dBm
    // else if (rssi_dBm >= -135) s_level = 2;  // S2  = -135 dBm
    // else if (rssi_dBm >= -141) s_level = 1;  // S1  = -141 dBm
    // else                       s_level = 0;  // S0 (below -141 dBm)

    if (rssi_dBm >= -93)
        s_level = 9;
    else if (rssi_dBm < -141)
        s_level = 0;
    else 
        s_level = (rssi_dBm + 147) / 6;

    if (s_level == 9) {
        // Compute over-S9 dB directly
        overS9dBm  = (uint8_t)MIN(rssi_dBm - (-93), 40);
        overS9Bars = overS9dBm / 10;
    }
    const int16_t display_rssi_dBm = (rssi_dBm > -53) ? -53 : rssi_dBm;
#else
    const int16_t s0_dBm   = -gEeprom.S0_LEVEL;                  // S0 .. base level
    const int16_t rssi_dBm =
        BK4819_GetRSSI_dBm()
#ifdef ENABLE_AM_FIX
        + ((gSetting_AM_fix && gRxVfo->Modulation == MODULATION_AM) ? AM_fix_get_gain_diff() : 0)
#endif
        + dBmCorrTable[gRxVfo->Band];

    int s0_9 = gEeprom.S0_LEVEL - gEeprom.S9_LEVEL;
    const uint8_t s_level = MIN(MAX((int32_t)(rssi_dBm - s0_dBm)*100 / (s0_9*100/9), 0), 9); // S0 - S9
    uint8_t overS9dBm = MIN(MAX(rssi_dBm + gEeprom.S9_LEVEL, 0), 99);
    uint8_t overS9Bars = MIN(overS9dBm/10, 4);
#endif

#ifdef ENABLE_FEAT_F4HWN
    if (gSetting_set_gui)
    {
        sprintf(str, "%3d", display_rssi_dBm);
        UI_PrintStringSmallNormal(str, LCD_WIDTH + 8, 0, line - 1);
    }
    else
    {
        sprintf(str, "% 4d %s", display_rssi_dBm, "dBm");
        if(isMainOnly())
            GUI_DisplaySmallest(str, 2, 41, false, true);
        else
            GUI_DisplaySmallest(str, 2, 25, false, true);
    }

    if(overS9Bars == 0) {
        sprintf(str, "S%d", s_level);
    }
    else {
        sprintf(str, "+%02d", overS9dBm);
    }

    UI_PrintStringSmallNormal(str, LCD_WIDTH + 38, 0, line - 1);
#else
    if(overS9Bars == 0) {
        sprintf(str, "% 4d S%d", -rssi_dBm, s_level);
    }
    else {
        sprintf(str, "% 4d  %2d", -rssi_dBm, overS9dBm);
        memcpy(p_line + 2 + 7*5, &plus, ARRAY_SIZE(plus));
    }

    UI_PrintStringSmallNormal(str, 2, 0, line);
#endif
    DrawLevelBar(bar_x, line, s_level + overS9Bars, 13);
#ifdef ENABLE_FEAT_F4HWN
    if (now && memcmp(oldLine, p_line, LCD_WIDTH) != 0)
        ST7565_BlitLine(line);
#else
    if (now)
        ST7565_BlitLine(line);
#endif
#else
    int16_t rssi = BK4819_GetRSSI();
    uint8_t Level;

    if (rssi >= gEEPROM_RSSI_CALIB[gRxVfo->Band][3]) {
        Level = 6;
    } else if (rssi >= gEEPROM_RSSI_CALIB[gRxVfo->Band][2]) {
        Level = 4;
    } else if (rssi >= gEEPROM_RSSI_CALIB[gRxVfo->Band][1]) {
        Level = 2;
    } else if (rssi >= gEEPROM_RSSI_CALIB[gRxVfo->Band][0]) {
        Level = 1;
    } else {
        Level = 0;
    }

    uint8_t *pLine = (gEeprom.RX_VFO == 0)? gFrameBuffer[2] : gFrameBuffer[6];
    if (now)
        memset(pLine, 0, 23);
    DrawSmallPowerBars(pLine, Level);
    if (now)
        ST7565_BlitFullScreen();
#endif

}

#ifdef ENABLE_AGC_SHOW_DATA
void UI_MAIN_PrintAGC(bool now)
{
    char buf[20];
    memset(gFrameBuffer[3], 0, 128);
    union {
        struct {
            uint16_t _ : 5;
            uint16_t agcSigStrength : 7;
            int16_t gainIdx : 3;
            uint16_t agcEnab : 1;
        };
        uint16_t __raw;
    } reg7e;
    reg7e.__raw = BK4819_ReadRegister(0x7E);
    uint8_t gainAddr = reg7e.gainIdx < 0 ? 0x14 : 0x10 + reg7e.gainIdx;
    union {
        struct {
            uint16_t pga:3;
            uint16_t mixer:2;
            uint16_t lna:3;
            uint16_t lnaS:2;
        };
        uint16_t __raw;
    } agcGainReg;
    agcGainReg.__raw = BK4819_ReadRegister(gainAddr);
    int8_t lnaShortTab[] = {-28, -24, -19, 0};
    int8_t lnaTab[] = {-24, -19, -14, -9, -6, -4, -2, 0};
    int8_t mixerTab[] = {-8, -6, -3, 0};
    int8_t pgaTab[] = {-33, -27, -21, -15, -9, -6, -3, 0};
    int16_t agcGain = lnaShortTab[agcGainReg.lnaS] + lnaTab[agcGainReg.lna] + mixerTab[agcGainReg.mixer] + pgaTab[agcGainReg.pga];

    sprintf(buf, "%d%2d %2d %2d %3d", reg7e.agcEnab, reg7e.gainIdx, -agcGain, reg7e.agcSigStrength, BK4819_GetRSSI());
    UI_PrintStringSmallNormal(buf, 2, 0, 3);
    if(now)
        ST7565_BlitLine(3);
}
#endif

void UI_MAIN_TimeSlice500ms(void)
{
    if(gScreenToDisplay==DISPLAY_MAIN) {
#ifdef ENABLE_FEAT_F4HWN_SCAN_PROGRESS
        if (gScanListNameCountdown_500ms > 0 && --gScanListNameCountdown_500ms == 0)
            gUpdateDisplay = true;
#endif
#ifdef ENABLE_AGC_SHOW_DATA
        UI_MAIN_PrintAGC(true);
        return;
#endif

#ifdef ENABLE_FEAT_F4HWN // Blink Green Led for white...
        if (!FUNCTION_IsRx() && gSetting_set_eot > 0 && RxBlinkLed == 2)
        {
            if(RxBlinkLedCounter <= 8)
            {
                if(RxBlinkLedCounter % 2 == 0)
                {
                    if(gSetting_set_eot > 1 )
                    {
                        BK4819_ToggleGpioOut(BK4819_GPIO6_PIN2_GREEN, false);
                    }
                }
                else
                {
                    if(gSetting_set_eot > 1 )
                    {
                        BK4819_ToggleGpioOut(BK4819_GPIO6_PIN2_GREEN, true);
                    }

                    if(gSetting_set_eot == 1 || gSetting_set_eot == 3)
                    {
                        switch(RxBlinkLedCounter)
                        {
                            case 1:
                            AUDIO_PlayBeep(BEEP_400HZ_30MS);
                            break;

                            case 3:
                            AUDIO_PlayBeep(BEEP_400HZ_30MS);
                            break;

                            case 5:
                            AUDIO_PlayBeep(BEEP_500HZ_30MS);
                            break;

                            case 7:
                            AUDIO_PlayBeep(BEEP_600HZ_30MS);
                            break;
                        }
                    }
                }
                RxBlinkLedCounter += 1;
            }
            else
            {
                RxBlinkLed = 0;
            }
        }
#endif
    }
}

// ----------------------------------------

#ifdef ENABLE_FEAT_F4HWN_ACTION_PICKER
static void UI_PrintActionPickerLabel(uint8_t index, uint8_t line, bool big)
{
    char label[20];
    strcpy(label, gSubMenu_SIDEFUNCTIONS[index].name);

    char *newline = strchr(label, '\n');
    if (newline != NULL)
        *newline = ' ';

    if (big)
        UI_PrintString(label, 0, LCD_WIDTH, line, 8);
    else
        UI_PrintStringSmallNormal(label, 0, LCD_WIDTH, line);
}
#endif

void UI_DisplayMain(void)
{
	center_line = CENTER_LINE_IN_USE;

	/* LOW BATTERY / keypad-lock hints are drawn in the home wave row */

#ifdef ENABLE_FEAT_F4HWN_ACTION_PICKER
	if (gActionPickerKey != 0) {
		const uint8_t selection = gActionPickerSelection[gActionPickerKey - 1];
		uint8_t previous = selection - 1;
		uint8_t next = selection + 1;

		if (previous == 0)
			previous = gSubMenu_SIDEFUNCTIONS_size - 1;
		if (next >= gSubMenu_SIDEFUNCTIONS_size)
			next = 1;

		UI_DisplayClear();
		UI_PrintActionPickerLabel(previous, 1, false);
		UI_PrintActionPickerLabel(selection, 2, true);
		UI_PrintActionPickerLabel(next, 4, false);
		ST7565_BlitFullScreen();
		return;
	}
#endif

	UI_DisplaySyrupHome();
	ST7565_BlitStatusLine();
	ST7565_BlitFullScreen();
}
