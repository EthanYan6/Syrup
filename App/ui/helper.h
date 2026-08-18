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

#ifndef UI_UI_H
#define UI_UI_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

void UI_GenerateChannelString(char *pString, const uint16_t Channel);
void UI_GenerateChannelStringEx(char *pString, const bool bShowPrefix, const uint16_t ChannelNumber);
void UI_PrintString(const char *pString, uint8_t Start, uint8_t End, uint8_t Line, uint8_t Width);
void UI_PrintStringSmallNormal(const char *pString, uint8_t Start, uint8_t End, uint8_t Line);
void UI_PrintStringSmallNormalInverse(const char *pString, uint8_t Start, uint8_t End, uint8_t Line);
void UI_PrintStringSmallBold(const char *pString, uint8_t Start, uint8_t End, uint8_t Line);
void UI_PrintStringSmallBufferNormal(const char *pString, uint8_t *buffer);
void UI_PrintStringSmallBufferBold(const char *pString, uint8_t * buffer);
void UI_DisplayFrequency(const char *string, uint8_t X, uint8_t Y, bool center);

void UI_DisplayPopup(const char *string);

void UI_DrawPixelBuffer(uint8_t (*buffer)[128], uint8_t x, uint8_t y, bool black);
#ifdef ENABLE_FEAT_F4HWN
    //void UI_DrawLineDottedBuffer(uint8_t (*buffer)[128], int16_t x1, int16_t y1, int16_t x2, int16_t y2, bool black);
    void PutPixel(uint8_t x, uint8_t y, bool fill);
    void PutPixelStatus(uint8_t x, uint8_t y, bool fill);
    void GUI_DisplaySmallest(const char *pString, uint8_t x, uint8_t y, bool statusbar, bool fill);
    void GUI_DisplaySmallestInverse(const char *pString, uint8_t x, uint8_t Line, bool statusbar, bool fill, uint8_t endX);
    void UI_DisplayUnlockKeyboard(uint8_t shift);
    bool IsEmptyName(const char *name, uint8_t len);
#endif
void UI_DrawLineBuffer(uint8_t (*buffer)[128], int16_t x1, int16_t y1, int16_t x2, int16_t y2, bool black);
void UI_DrawRectangleBuffer(uint8_t (*buffer)[128], int16_t x1, int16_t y1, int16_t x2, int16_t y2, bool black);

void UI_DisplayClear();
void UI_StatusClear();

#ifdef ENABLE_CHINESE
size_t UI_SmallStringPixelWidth(const char *pString);
/** One small-font line: 12px if the line has CJK, else 8px (Latin framebuffer row). */
uint8_t UI_SmallLinePixelHeight(const char *pString);
/** Top Y of 7px Latin glyphs inside [y_start, y_end] (inclusive).
 *  mixed_cjk: place Latin in the Han 12px box (optical vertical center).
 *  latin_down_when_mixed: 0 = centered with Han; >0 = extra pixels down (baseline tweak). */
uint8_t UI_SmallLatinPixelY(uint8_t y_pixel_start, uint8_t y_pixel_end, bool mixed_cjk, uint8_t latin_down_when_mixed);
void UI_PrintStringSmallAtPixel(const char *pString, uint8_t x_start, uint8_t x_end, uint8_t y_pixel_start, uint8_t y_pixel_end, uint8_t latin_down_when_mixed);
/** Stack lines in [y_pixel_start, y_pixel_end] (inclusive). Each line uses UI_SmallLinePixelHeight; gap_px between lines. */
void UI_PrintStringSmallStackedAtPixel(const char *const *lines, uint8_t n_lines, uint8_t x_start, uint8_t x_end, uint8_t y_pixel_start, uint8_t y_pixel_end, uint8_t gap_px, uint8_t latin_down_when_mixed);
/** Key-lock unlock hint: mixed CJK+Latin; optical center (latin_down=0) */
void UI_PrintStringSmallAtPixelKeyLockUnlockHint(const char *pString, uint8_t x_start, uint8_t x_end, uint8_t y_pixel_start, uint8_t y_pixel_end);
/** 12px vertical band for channel names (CJK+Latin mix), latin_down=0 (optical center) */
void UI_PrintStringSmallChannelNameBand(const char *pString, uint8_t x_start, uint8_t x_end, uint8_t y_pixel_top);
void UI_PrintStringSmallAtPixelInverse(const char *pString, uint8_t x_start, uint8_t x_end, uint8_t y_pixel_start, uint8_t y_pixel_end);
void UI_PrintStringSmallAtPixelCnInverse(const char *pString, uint8_t x_start, uint8_t x_end, uint8_t y_pixel_start, uint8_t y_pixel_end);
#endif

#endif
