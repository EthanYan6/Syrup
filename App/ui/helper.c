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

#include "driver/st7565.h"
#include "external/printf/printf.h"
#include "font.h"
#include "ui/helper.h"
#include "ui/inputbox.h"
#include "misc.h"
#include "settings.h"


void UI_GenerateChannelString(char *pString, const uint16_t Channel)
{
    unsigned int i;

    if (gInputBoxIndex == 0)
    {
        sprintf(pString, "CH-%02u", Channel + 1);
        return;
    }

    pString[0] = 'C';
    pString[1] = 'H';
    pString[2] = '-';
    for (i = 0; i < 2; i++)
        pString[i + 3] = (gInputBox[i] == 10) ? '-' : gInputBox[i] + '0';

    pString[5] = 0;
}

void UI_GenerateChannelStringEx(char *pString, const bool bShowPrefix, const uint16_t ChannelNumber)
{
    if (gInputBoxIndex > 0) {
        for (unsigned int i = 0; i < 4; i++) {
            pString[i] = (gInputBox[i] == 10) ? '-' : gInputBox[i] + '0';
        }

        pString[4] = 0;
        return;
    }

    if (bShowPrefix) {
        // BUG here? Prefixed NULLs are allowed
        sprintf(pString, "CH-%04u", ChannelNumber + 1);
    } else if (ChannelNumber == MR_CHANNEL_LAST + 1) {
        strcpy(pString, "None");
    } else if (ChannelNumber == 0xFFFF) {
        strcpy(pString, "NULL");
    } else {
        sprintf(pString, "%04u", ChannelNumber + 1);
    }
}

void UI_PrintStringBuffer(const char *pString, uint8_t * buffer, uint32_t char_width, const uint8_t *font)
{
    const size_t Length = strlen(pString);
    const unsigned int char_spacing = char_width + 1;
    for (size_t i = 0; i < Length; i++) {
        const unsigned int index = pString[i] - ' ' - 1;
        if (pString[i] > ' ' && pString[i] < 127) {
            const uint32_t offset = i * char_spacing + 1;
            memcpy(buffer + offset, font + index * char_width, char_width);
        }
    }
}

void UI_PrintString(const char *pString, uint8_t Start, uint8_t End, uint8_t Line, uint8_t Width)
{
    size_t i;
    size_t Length = strlen(pString);

    if (End > Start)
        Start += (((End - Start) - (Length * Width)) + 1) / 2;

    for (i = 0; i < Length; i++)
    {
        const unsigned int ofs   = (unsigned int)Start + (i * Width);
        if (pString[i] > ' ' && pString[i] < 127)
        {
            const unsigned int index = pString[i] - ' ' - 1;
            memcpy(gFrameBuffer[Line + 0] + ofs, &gFontBig[index][0], 7);
            memcpy(gFrameBuffer[Line + 1] + ofs, &gFontBig[index][7], 7);
        }
    }
}

void UI_PrintStringSmall(const char *pString, uint8_t Start, uint8_t End, uint8_t Line, uint8_t char_width, const uint8_t *font)
{
    const size_t Length = strlen(pString);
    const unsigned int char_spacing = char_width + 1;

    if (End > Start) {
        Start += (((End - Start) - Length * char_spacing) + 1) / 2;
    }

    UI_PrintStringBuffer(pString, gFrameBuffer[Line] + Start, char_width, font);
}


void UI_PrintStringSmallNormal(const char *pString, uint8_t Start, uint8_t End, uint8_t Line)
{
    UI_PrintStringSmall(pString, Start, End, Line, ARRAY_SIZE(gFontSmall[0]), (const uint8_t *)gFontSmall);
}

void UI_PrintStringSmallNormalInverse(const char *pString, uint8_t Start, uint8_t End, uint8_t Line)
{
    // First draw the string normally
    UI_PrintStringSmallNormal(pString, Start, End, Line);

    // Now invert the framebuffer bits for the rendered area
    uint8_t len = strlen(pString);
    uint8_t char_width = 7; // small font is typically 6px wide

    uint8_t x_start = Start;
    uint8_t x_end   = Start + (len * char_width) + 1;

    if (End != 0 && x_end > End)
        x_end = End;

    //gFrameBuffer[Line][x_start - 2] ^= 0x3E;
    gFrameBuffer[Line][x_start - 1] ^= 0x7F;
    //gFrameBuffer[Line][x_start - 1] ^= 0xFF;
    for (uint8_t x = x_start; x < x_end; x++)
    {
        gFrameBuffer[Line][x] ^= 0xFF;
        gFrameBuffer[Line - 1][x] ^= 0x80;
    }
    //gFrameBuffer[Line][x_end + 0] ^= 0xFF;
    gFrameBuffer[Line][x_end + 0] ^= 0x7F;
    //gFrameBuffer[Line][x_end + 1] ^= 0x3E;
}


void UI_PrintStringSmallBold(const char *pString, uint8_t Start, uint8_t End, uint8_t Line)
{
#ifdef ENABLE_SMALL_BOLD
    const uint8_t *font = (uint8_t *)gFontSmallBold;
    const uint8_t char_width = ARRAY_SIZE(gFontSmallBold[0]);
#else
    const uint8_t *font = (uint8_t *)gFontSmall;
    const uint8_t char_width = ARRAY_SIZE(gFontSmall[0]);
#endif

    UI_PrintStringSmall(pString, Start, End, Line, char_width, font);
}

void UI_PrintStringSmallBufferNormal(const char *pString, uint8_t * buffer)
{
    UI_PrintStringBuffer(pString, buffer, ARRAY_SIZE(gFontSmall[0]), (uint8_t *)gFontSmall);
}

void UI_PrintStringSmallBufferBold(const char *pString, uint8_t * buffer)
{
#ifdef ENABLE_SMALL_BOLD
    const uint8_t *font = (uint8_t *)gFontSmallBold;
    const uint8_t char_width = ARRAY_SIZE(gFontSmallBold[0]);
#else
    const uint8_t *font = (uint8_t *)gFontSmall;
    const uint8_t char_width = ARRAY_SIZE(gFontSmall[0]);
#endif
    UI_PrintStringBuffer(pString, buffer, char_width, font);
}

void UI_DisplayFrequency(const char *string, uint8_t X, uint8_t Y, bool center)
{
    const unsigned int char_width  = 13;
    uint8_t           *pFb0        = gFrameBuffer[Y] + X;
    uint8_t           *pFb1        = pFb0 + 128;
    bool               bCanDisplay = false;

    uint8_t len = strlen(string);
    for(int i = 0; i < len; i++) {
        char c = string[i];
        if(c=='-') c = '9' + 1;
        if (bCanDisplay || c != ' ')
        {
            bCanDisplay = true;
            if(c>='0' && c<='9' + 1) {
                memcpy(pFb0 + 2, gFontBigDigits[c-'0'],                  char_width - 3);
                memcpy(pFb1 + 2, gFontBigDigits[c-'0'] + char_width - 3, char_width - 3);
            }
            else if(c=='.') {
                *pFb1 = 0x60; pFb0++; pFb1++;
                *pFb1 = 0x60; pFb0++; pFb1++;
                *pFb1 = 0x60; pFb0++; pFb1++;
                continue;
            }

        }
        else if (center) {
            pFb0 -= 6;
            pFb1 -= 6;
        }
        pFb0 += char_width;
        pFb1 += char_width;
    }
}

/*
void UI_DisplayFrequency(const char *string, uint8_t X, uint8_t Y, bool center)
{
    const unsigned int char_width  = 13;
    uint8_t           *pFb0        = gFrameBuffer[Y] + X;
    uint8_t           *pFb1        = pFb0 + 128;
    bool               bCanDisplay = false;

    if (center) {
        uint8_t len = 0;
        for (const char *ptr = string; *ptr; ptr++)
            if (*ptr != ' ') len++; // Ignores spaces for centering

        X -= (len * char_width) / 2; // Centering adjustment
        pFb0 = gFrameBuffer[Y] + X;
        pFb1 = pFb0 + 128;
    }

    for (; *string; string++) {
        char c = *string;
        if (c == '-') c = '9' + 1; // Remap of '-' symbol

        if (bCanDisplay || c != ' ') {
            bCanDisplay = true;
            if (c >= '0' && c <= '9' + 1) {
                memcpy(pFb0 + 2, gFontBigDigits[c - '0'], char_width - 3);
                memcpy(pFb1 + 2, gFontBigDigits[c - '0'] + char_width - 3, char_width - 3);
            } else if (c == '.') {
                memset(pFb1, 0x60, 3); // Replaces the three assignments
                pFb0 += 3;
                pFb1 += 3;
                continue;
            }
        }
        pFb0 += char_width;
        pFb1 += char_width;
    }
}
*/

void UI_DrawPixelBuffer(uint8_t (*buffer)[128], uint8_t x, uint8_t y, bool black)
{
    const uint8_t pattern = 1 << (y % 8);
    if(black)
        buffer[y/8][x] |= pattern;
    else
        buffer[y/8][x] &= ~pattern;
}

static void sort(int16_t *a, int16_t *b)
{
    if(*a > *b) {
        int16_t t = *a;
        *a = *b;
        *b = t;
    }
}

#ifdef ENABLE_FEAT_F4HWN
    /*
    void UI_DrawLineDottedBuffer(uint8_t (*buffer)[128], int16_t x1, int16_t y1, int16_t x2, int16_t y2, bool black)
    {
        if(x2==x1) {
            sort(&y1, &y2);
            for(int16_t i = y1; i <= y2; i+=2) {
                UI_DrawPixelBuffer(buffer, x1, i, black);
            }
        } else {
            const int multipl = 1000;
            int a = (y2-y1)*multipl / (x2-x1);
            int b = y1 - a * x1 / multipl;

            sort(&x1, &x2);
            for(int i = x1; i<= x2; i+=2)
            {
                UI_DrawPixelBuffer(buffer, i, i*a/multipl +b, black);
            }
        }
    }
    */

    void PutPixel(uint8_t x, uint8_t y, bool fill) {
      UI_DrawPixelBuffer(gFrameBuffer, x, y, fill);
    }

    void PutPixelStatus(uint8_t x, uint8_t y, bool fill) {
      UI_DrawPixelBuffer(&gStatusLine, x, y, fill);
    }

    void GUI_DisplaySmallest(const char *pString, uint8_t x, uint8_t y,
                                    bool statusbar, bool fill) {
      uint8_t c;
      uint8_t pixels;
      const uint8_t *p = (const uint8_t *)pString;

      while ((c = *p++) && c != '\0') {
        c -= 0x20;
        for (int i = 0; i < 3; ++i) {
          pixels = gFont3x5[c][i];
          for (int j = 0; j < 6; ++j) {
            if (pixels & 1) {
              if (statusbar)
                PutPixelStatus(x + i, y + j, fill);
              else
                PutPixel(x + i, y + j, fill);
            }
            pixels >>= 1;
          }
        }
        x += 4;
      }
    }

    void GUI_DisplaySmallestInverse(const char *pString, uint8_t x, uint8_t Line,
                                bool statusbar, bool fill, uint8_t end)
    {
        // First draw the string normally
        GUI_DisplaySmallest(pString, x, (Line * 8) + 1, statusbar, fill);

        // Now invert the framebuffer/statusline bits for the rendered area
        uint8_t start = (x - 2);
        uint8_t *buffer = statusbar ? gStatusLine : gFrameBuffer[Line];

        buffer[start] ^= 0x3E;
        for (uint8_t i = start + 1; i < end; i++) {
            buffer[i] ^= 0x7F;
        }
        buffer[end] ^= 0x3E;
    }

    void UI_DisplayUnlockKeyboard(uint8_t shift) {
        if (gEeprom.KEY_LOCK && gKeypadLocked > 0)
        {   // tell user how to unlock the keyboard
            
            //memcpy(gFrameBuffer[shift] + 2, gFontKeyLock, sizeof(gFontKeyLock));
#ifdef ENABLE_CHINESE
            if (gUiLanguage == UI_LANGUAGE_CN) {
                const uint8_t y0 = (uint8_t)(shift * 8u);
                UI_PrintStringSmallAtPixelKeyLockUnlockHint("长按#解锁", 0, LCD_WIDTH - 1u, y0, (uint8_t)(y0 + 11u));
                return;
            }
#endif
            UI_PrintStringSmallBold("UNLOCK KEYBOARD", 12, 0, shift);
            //memcpy(gFrameBuffer[shift] + 120, gFontKeyLock, sizeof(gFontKeyLock));

            /*
            for (uint8_t i = 12; i < 116; i++)
            {
                gFrameBuffer[shift][i] ^= 0xFF;
            }
            */
        }
    }

    bool IsEmptyName(const char *name, uint8_t len) {
        if (name[0] == '\0' || name[0] == '\xff')
            return true;
        for (uint8_t i = 0; i < len; i++) {
            if (name[i] != ' ' && name[i] != '\xff' && name[i] != '\0')
                return false;
        }
        return true;
    }
#endif
    
void UI_DrawLineBuffer(uint8_t (*buffer)[128], int16_t x1, int16_t y1, int16_t x2, int16_t y2, bool black)
{
    if(x2==x1) {
        sort(&y1, &y2);
        for(int16_t i = y1; i <= y2; i++) {
            UI_DrawPixelBuffer(buffer, x1, i, black);
        }
    } else {
        const int multipl = 1000;
        int a = (y2-y1)*multipl / (x2-x1);
        int b = y1 - a * x1 / multipl;

        sort(&x1, &x2);
        for(int i = x1; i<= x2; i++)
        {
            UI_DrawPixelBuffer(buffer, i, i*a/multipl +b, black);
        }
    }
}

void UI_DrawRectangleBuffer(uint8_t (*buffer)[128], int16_t x1, int16_t y1, int16_t x2, int16_t y2, bool black)
{
    UI_DrawLineBuffer(buffer, x1,y1, x1,y2, black);
    UI_DrawLineBuffer(buffer, x1,y1, x2,y1, black);
    UI_DrawLineBuffer(buffer, x2,y1, x2,y2, black);
    UI_DrawLineBuffer(buffer, x1,y2, x2,y2, black);
}


void UI_DisplayPopup(const char *string)
{
    UI_DisplayClear();

    // for(uint8_t i = 1; i < 5; i++) {
    //  memset(gFrameBuffer[i]+8, 0x00, 111);
    // }

    // for(uint8_t x = 10; x < 118; x++) {
    //  UI_DrawPixelBuffer(x, 10, true);
    //  UI_DrawPixelBuffer(x, 46-9, true);
    // }

    // for(uint8_t y = 11; y < 37; y++) {
    //  UI_DrawPixelBuffer(10, y, true);
    //  UI_DrawPixelBuffer(117, y, true);
    // }
    // DrawRectangle(9,9, 118,38, true);
    UI_PrintString(string, 9, 118, 2, 8);
    UI_PrintStringSmallNormal("Press EXIT", 9, 118, 6);
}

void UI_DisplayClear()
{
    memset(gFrameBuffer, 0, sizeof(gFrameBuffer));
}

void UI_StatusClear()
{
    memset(gStatusLine, 0, sizeof(gStatusLine));
}

#ifdef ENABLE_CHINESE

/* Syrup: y is framebuffer-relative (0 = top of gFrameBuffer), unlike Dondji's
 * screen-absolute Y that subtracts 8 for the status line. */

static bool IsChineseChar(const char *pStr)
{
    uint8_t c = (uint8_t)pStr[0];
    return (c >= 0xE4 && c <= 0xEF);
}

static uint16_t Utf8ToUnicode(const char *pStr)
{
    uint8_t c1 = (uint8_t)pStr[0];
    uint8_t c2 = (uint8_t)pStr[1];
    uint8_t c3 = (uint8_t)pStr[2];
    return ((c1 & 0x0F) << 12) | ((c2 & 0x3F) << 6) | (c3 & 0x3F);
}

static void DrawChineseChar(uint16_t unicode, uint8_t x, uint8_t y_pixel, uint8_t y_pixel_end)
{
    int16_t spi_index = SETTINGS_CNCharToIndex(unicode);
    if (spi_index < 0)
        return;

    const uint8_t char_height = 12;
    const uint8_t char_width = 12;
    const uint16_t y_range = (uint16_t)y_pixel_end - (uint16_t)y_pixel + 1u;
    uint8_t y_offset = 0;
    if (y_range >= char_height)
        y_offset = (uint8_t)((y_range - char_height) / 2u);
    uint8_t y = y_pixel + y_offset;

    uint16_t spi_bitmap[12];
    SETTINGS_ReadCNFontBitmap((uint16_t)spi_index, spi_bitmap);

    for (uint8_t row = 0; row < char_height; row++) {
        uint16_t row_data = spi_bitmap[row];
        uint8_t current_y = y + row;
        uint8_t line = current_y / 8;
        uint8_t bit_offset = current_y % 8;
        if (line >= FRAME_LINES)
            break;
        for (uint8_t col = 0; col < char_width; col++) {
            if (x + col >= LCD_WIDTH)
                break;
            if (row_data & (0x8000 >> col))
                gFrameBuffer[line][x + col] |= (1 << bit_offset);
        }
    }
}

size_t UI_SmallStringPixelWidth(const char *pString)
{
    const uint8_t eng_char_width = 6;
    const uint8_t chn_char_width = 12;
    size_t        total_width    = 0;
    size_t        i              = 0;
    while (pString[i]) {
        if (IsChineseChar(&pString[i])) {
            total_width += chn_char_width + 1;
            i += 3;
        } else {
            total_width += eng_char_width + 1;
            i++;
        }
    }
    if (total_width > 0)
        total_width--;
    return total_width;
}

uint8_t UI_SmallLinePixelHeight(const char *pString)
{
    const uint8_t latin_h = 8u;
    const uint8_t cjk_h   = 12u;
    size_t        i;

    if (pString == NULL || pString[0] == '\0')
        return latin_h;

    i = 0;
    while (pString[i]) {
        if (IsChineseChar(&pString[i]))
            return cjk_h;
        i++;
    }
    return latin_h;
}

void UI_PrintStringSmallStackedAtPixel(const char *const *lines, uint8_t n_lines, uint8_t x_start, uint8_t x_end, uint8_t y_pixel_start, uint8_t y_pixel_end, uint8_t gap_px, uint8_t latin_down_when_mixed)
{
    uint8_t  heights[FRAME_LINES];
    uint8_t  i;
    uint8_t  n;
    uint16_t block_h;
    uint16_t area_h;
    uint8_t  y;

    if (lines == NULL || n_lines == 0u)
        return;
    if (y_pixel_end < y_pixel_start)
        return;

    n = n_lines;
    if (n > FRAME_LINES)
        n = FRAME_LINES;

    block_h = 0u;
    for (i = 0; i < n; i++) {
        heights[i] = UI_SmallLinePixelHeight(lines[i]);
        block_h = (uint16_t)(block_h + heights[i]);
        if (i > 0u)
            block_h = (uint16_t)(block_h + gap_px);
    }

    area_h = (uint16_t)y_pixel_end - (uint16_t)y_pixel_start + 1u;
    if (block_h <= area_h)
        y = (uint8_t)(y_pixel_start + (uint8_t)((area_h - block_h) / 2u));
    else
        y = y_pixel_start;

    for (i = 0; i < n; i++) {
        if (lines[i] != NULL && lines[i][0] != '\0') {
            const uint8_t y1 = (uint8_t)(y + heights[i] - 1u);

            UI_PrintStringSmallAtPixel(lines[i], x_start, x_end, y, y1, latin_down_when_mixed);
        }
        y = (uint8_t)(y + heights[i] + gap_px);
    }
}

uint8_t UI_SmallLatinPixelY(uint8_t y_pixel_start, uint8_t y_pixel_end, bool mixed_cjk, uint8_t latin_down_when_mixed)
{
    const uint8_t eng_char_height = 7;
    const uint8_t chn_char_height = 12;
    const uint16_t y_range = (uint16_t)y_pixel_end - (uint16_t)y_pixel_start + 1u;
    uint8_t y_pixel;

    if (mixed_cjk) {
        /* Sit Latin in the same 12px box as Han so mixed lines share one optical center.
         * latin_down_when_mixed==0: pure vertical center; >0: extra baseline-style drop. */
        uint8_t chn_top = y_pixel_start;
        if (y_range >= chn_char_height)
            chn_top = (uint8_t)(y_pixel_start + (uint8_t)((y_range - chn_char_height) / 2u));
        y_pixel = (uint8_t)(chn_top + (uint8_t)((chn_char_height - eng_char_height) / 2u));
        if (latin_down_when_mixed > 0u) {
            if (y_pixel <= (uint8_t)(255u - latin_down_when_mixed))
                y_pixel = (uint8_t)(y_pixel + latin_down_when_mixed);
        }
    } else {
        unsigned y_offset = 0;
        if (y_range >= eng_char_height)
            y_offset = (unsigned)((y_range - eng_char_height) / 2u);
        if (y_range >= eng_char_height) {
            const unsigned max_off = (unsigned)(y_range - eng_char_height);
            if (y_offset > max_off)
                y_offset = max_off;
        }
        y_pixel = (uint8_t)(y_pixel_start + (uint8_t)y_offset);
    }
    return y_pixel;
}

void UI_PrintStringSmallAtPixel(const char *pString, uint8_t x_start, uint8_t x_end, uint8_t y_pixel_start, uint8_t y_pixel_end, uint8_t latin_down_when_mixed)
{
    const uint8_t eng_char_width = 6;
    const uint8_t eng_char_height = 7;
    const uint8_t chn_char_width = 12;
    size_t total_width = 0;
    size_t i = 0;
    bool has_chinese = false;
    while (pString[i]) {
        if (IsChineseChar(&pString[i])) {
            has_chinese = true;
            total_width += chn_char_width + 1;
            i += 3;
        } else {
            total_width += eng_char_width + 1;
            i++;
        }
    }
    if (total_width > 0)
        total_width--;
    uint8_t x = x_start;
    if (x_end > x_start && total_width < (x_end - x_start))
        x += (x_end - x_start - total_width) / 2;
    i = 0;
    while (pString[i]) {
        if (IsChineseChar(&pString[i])) {
            uint16_t unicode = Utf8ToUnicode(&pString[i]);
            DrawChineseChar(unicode, x, y_pixel_start, y_pixel_end);
            x += chn_char_width + 1;
            i += 3;
        } else {
            uint8_t y_pixel = UI_SmallLatinPixelY(y_pixel_start, y_pixel_end, has_chinese, latin_down_when_mixed);
            uint8_t line = y_pixel / 8;
            uint8_t bit_offset = y_pixel % 8;
            if (line < FRAME_LINES && pString[i] >= '!' && pString[i] < 127) {
                const unsigned int index = pString[i] - ' ' - 1;
                if (index < ARRAY_SIZE(gFontSmall)) {
                    const uint8_t *font_data = gFontSmall[index];
                    for (uint8_t col = 0; col < eng_char_width; col++) {
                        if (x + col >= LCD_WIDTH)
                            break;
                        uint8_t pixel_col = font_data[col];
                        gFrameBuffer[line][x + col] |= (pixel_col << bit_offset);
                        if (bit_offset + eng_char_height > 8 && line + 1 < FRAME_LINES)
                            gFrameBuffer[line + 1][x + col] |= (pixel_col >> (8 - bit_offset));
                    }
                }
            }
            x += eng_char_width + 1;
            i++;
        }
    }
}

void UI_PrintStringSmallAtPixelKeyLockUnlockHint(const char *pString, uint8_t x_start, uint8_t x_end, uint8_t y_pixel_start, uint8_t y_pixel_end)
{
    const uint8_t latin_down_key_lock_unlock_hint = 0u;

    UI_PrintStringSmallAtPixel(pString, x_start, x_end, y_pixel_start, y_pixel_end, latin_down_key_lock_unlock_hint);
}

void UI_PrintStringSmallChannelNameBand(const char *pString, uint8_t x_start, uint8_t x_end, uint8_t y_pixel_top)
{
    const uint8_t band_bottom_inclusive = (uint8_t)((unsigned)y_pixel_top + 11u);

    UI_PrintStringSmallAtPixel(pString, x_start, x_end, y_pixel_top, band_bottom_inclusive, 0u);
}

void UI_PrintStringSmallAtPixelInverse(const char *pString, uint8_t x_start, uint8_t x_end, uint8_t y_pixel_start, uint8_t y_pixel_end)
{
    const uint8_t eng_char_width = 6;
    const uint8_t eng_char_height = 7;
    const uint8_t chn_char_width = 12;
    size_t total_width = 0;
    size_t i = 0;
    bool has_chinese = false;
    while (pString[i]) {
        if (IsChineseChar(&pString[i])) {
            has_chinese = true;
            total_width += chn_char_width + 1;
            i += 3;
        } else {
            total_width += eng_char_width + 1;
            i++;
        }
    }
    if (total_width > 0)
        total_width--;
    uint8_t x = x_start;
    if (x_end > x_start && total_width < (x_end - x_start))
        x += (x_end - x_start - total_width) / 2;
    i = 0;
    while (pString[i]) {
        if (IsChineseChar(&pString[i])) {
            uint16_t unicode = Utf8ToUnicode(&pString[i]);
            int16_t spi_idx = SETTINGS_CNCharToIndex(unicode);
            if (spi_idx >= 0) {
                uint16_t spi_bitmap[12];
                SETTINGS_ReadCNFontBitmap((uint16_t)spi_idx, spi_bitmap);
                const uint16_t y_range_inv = (uint16_t)y_pixel_end - (uint16_t)y_pixel_start + 1u;
                const uint8_t chn_top = (y_range_inv >= 12u)
                    ? (uint8_t)(y_pixel_start + (uint8_t)((y_range_inv - 12u) / 2u))
                    : y_pixel_start;
                for (uint8_t row = 0; row < 12; row++) {
                    uint16_t row_data = spi_bitmap[row];
                    uint8_t y = chn_top + row;
                    uint8_t line = y / 8;
                    uint8_t bit_offset = y % 8;
                    if (line >= FRAME_LINES)
                        break;
                    for (uint8_t col = 0; col < 12; col++) {
                        if (x + col >= LCD_WIDTH)
                            break;
                        if (row_data & (0x8000 >> col))
                            gFrameBuffer[line][x + col] &= ~(1 << bit_offset);
                    }
                }
            }
            x += chn_char_width + 1;
            i += 3;
        } else {
            const uint16_t y_range = (uint16_t)y_pixel_end - (uint16_t)y_pixel_start + 1u;
            unsigned y_offset = 0;
            if (y_range >= eng_char_height)
                y_offset = (unsigned)((y_range - eng_char_height) / 2u);
            if (y_range >= eng_char_height) {
                const unsigned max_off = (unsigned)(y_range - eng_char_height);
                if (y_offset > max_off)
                    y_offset = max_off;
            }
            uint8_t y_pixel = y_pixel_start + (uint8_t)y_offset;
            if (has_chinese) {
                if (y_pixel >= y_pixel_start + 4u)
                    y_pixel -= 4u;
                else if (y_pixel > y_pixel_start)
                    y_pixel = y_pixel_start;
            }
            uint8_t line = y_pixel / 8;
            uint8_t bit_offset = y_pixel % 8;
            if (line < FRAME_LINES && pString[i] >= '!' && pString[i] < 127) {
                const unsigned int char_index = pString[i] - ' ' - 1;
                if (char_index < ARRAY_SIZE(gFontSmall)) {
                    const uint8_t *font_data = gFontSmall[char_index];
                    for (uint8_t col = 0; col < eng_char_width; col++) {
                        if (x + col >= LCD_WIDTH)
                            break;
                        uint8_t pixel_col = font_data[col];
                        gFrameBuffer[line][x + col] &= ~(pixel_col << bit_offset);
                        if (bit_offset + eng_char_height > 8 && line + 1 < FRAME_LINES)
                            gFrameBuffer[line + 1][x + col] &= ~(pixel_col >> (8 - bit_offset));
                    }
                }
            }
            x += eng_char_width + 1;
            i++;
        }
    }
}

void UI_PrintStringSmallAtPixelCnInverse(const char *pString, uint8_t x_start, uint8_t x_end, uint8_t y_pixel_start, uint8_t y_pixel_end)
{
    const uint8_t chn_char_width = 12;
    size_t total_width = 0;
    size_t i = 0;
    while (pString[i]) {
        if (IsChineseChar(&pString[i])) {
            total_width += chn_char_width + 1;
            i += 3;
        } else {
            total_width += 7;
            i++;
        }
    }
    if (total_width > 0)
        total_width--;
    uint8_t x = x_start;
    if (x_end > x_start && total_width < (x_end - x_start))
        x += (uint8_t)((x_end - x_start - total_width) / 2);

    for (uint8_t yy = y_pixel_start; yy <= y_pixel_end; yy++)
        for (uint8_t xx = x_start; xx <= x_end && xx < LCD_WIDTH; xx++)
            UI_DrawPixelBuffer(gFrameBuffer, xx, yy, true);

    i = 0;
    while (pString[i]) {
        if (IsChineseChar(&pString[i])) {
            uint16_t unicode = Utf8ToUnicode(&pString[i]);
            int16_t spi_index = SETTINGS_CNCharToIndex(unicode);
            if (spi_index >= 0) {
                uint16_t spi_bitmap[12];
                SETTINGS_ReadCNFontBitmap((uint16_t)spi_index, spi_bitmap);
                const uint16_t y_range = (uint16_t)y_pixel_end - (uint16_t)y_pixel_start + 1u;
                uint8_t y_off = 0;
                if (y_range >= 12u)
                    y_off = (uint8_t)((y_range - 12u) / 2u);
                uint8_t y_base = (uint8_t)(y_pixel_start + y_off);
                for (uint8_t row = 0; row < 12; row++) {
                    uint16_t row_data = spi_bitmap[row];
                    uint8_t y = y_base + row;
                    for (uint8_t col = 0; col < 12; col++) {
                        if (x + col >= LCD_WIDTH)
                            break;
                        if (row_data & (0x8000 >> col))
                            UI_DrawPixelBuffer(gFrameBuffer, (uint8_t)(x + col), y, false);
                    }
                }
            }
            x += chn_char_width + 1;
            i += 3;
        } else {
            const uint8_t eng_char_width = 6;
            const uint8_t eng_char_height = 7;
            const uint16_t y_range = (uint16_t)y_pixel_end - (uint16_t)y_pixel_start + 1u;
            unsigned y_offset = 0;
            if (y_range >= eng_char_height)
                y_offset = (unsigned)((y_range - eng_char_height) / 2u);
            uint8_t y_pixel = (uint8_t)(y_pixel_start + (uint8_t)y_offset);
            if (pString[i] >= '!' && pString[i] < 127) {
                const unsigned int index = pString[i] - ' ' - 1;
                if (index < ARRAY_SIZE(gFontSmall)) {
                    const uint8_t *font_data = gFontSmall[index];
                    for (uint8_t col = 0; col < eng_char_width; col++) {
                        if (x + col >= LCD_WIDTH)
                            break;
                        uint8_t pixel_col = font_data[col];
                        for (uint8_t row = 0; row < eng_char_height; row++) {
                            if (pixel_col & (1u << row))
                                UI_DrawPixelBuffer(gFrameBuffer, (uint8_t)(x + col), (uint8_t)(y_pixel + row), false);
                        }
                    }
                }
            }
            x += eng_char_width + 1;
            i++;
        }
    }
}

#endif /* ENABLE_CHINESE */
