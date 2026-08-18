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

#include <stdbool.h>
#include <string.h>
#include "app/scanner.h"
#include "dcs.h"
#include "driver/st7565.h"
#include "external/printf/printf.h"
#include "misc.h"
#include "settings.h"
#include "ui/helper.h"
#include "ui/scanner.h"
#include "ui/status.h"

void UI_DisplayScanner(void)
{
	char String[32];
	char *pPrintStr = String;
#ifdef ENABLE_CHINESE
	const bool cn = (gUiLanguage == UI_LANGUAGE_CN);
#endif

	UI_DisplayClear();

	/* 1st line */
	if (gScannerSaveState == SCAN_SAVE_CHANNEL) {
#ifdef ENABLE_CHINESE
		pPrintStr = cn ? "保存?" : "Save?";
#else
		pPrintStr = "Save?";
#endif
	} else if (gScannerSaveState == SCAN_SAVE_CHAN_SEL) {
#ifdef ENABLE_CHINESE
		if (cn) {
			strcpy(String, "保存:");
			UI_GenerateChannelStringEx(String + 7, gShowChPrefix, gScanChannel);
		} else
#endif
		{
			strcpy(String, "Save:");
			UI_GenerateChannelStringEx(String + 5, gShowChPrefix, gScanChannel);
		}
		pPrintStr = String;
	} else if ((gScanCssState < SCAN_CSS_STATE_FOUND) && ((gScanProgressIndicator & 1u) != 0)) {
		pPrintStr = "";
	} else if (gScanCssState == SCAN_CSS_STATE_OFF) {
#ifdef ENABLE_CHINESE
		pPrintStr = cn ? "检测频率" : "Search Freq";
#else
		pPrintStr = "Search Freq";
#endif
	} else if (gScanCssState == SCAN_CSS_STATE_SCANNING) {
#ifdef ENABLE_CHINESE
		pPrintStr = cn ? "破解亚音" : "Search Tone";
#else
		pPrintStr = "Search Tone";
#endif
	} else if (gScanCssState == SCAN_CSS_STATE_FOUND) {
#ifdef ENABLE_CHINESE
		pPrintStr = cn ? "扫描完成" : "Scan Complete";
#else
		pPrintStr = "Scan Complete";
#endif
	} else {
#ifdef ENABLE_CHINESE
		pPrintStr = cn ? "扫描失败" : "Scan Failed";
#else
		pPrintStr = "Scan Failed";
#endif
	}

#ifdef ENABLE_CHINESE
	if (cn)
		UI_PrintStringSmallAtPixel(pPrintStr, 2, 2, 8u, 19u, 0u);
	else
#endif
		UI_PrintString(pPrintStr, 2, 0, 1, 8);

	/* 2nd line */
	if (gScanSingleFrequency || (gScanCssState != SCAN_CSS_STATE_OFF && gScanCssState != SCAN_CSS_STATE_FAILED)) {
#ifdef ENABLE_CHINESE
		if (cn)
			sprintf(String, "频率:%u.%05u", gScanFrequency / 100000, gScanFrequency % 100000);
		else
#endif
			sprintf(String, "Freq:%u.%05u", gScanFrequency / 100000, gScanFrequency % 100000);
		pPrintStr = String;
	} else {
#ifdef ENABLE_CHINESE
		pPrintStr = cn ? "频率:---.-----" : "Freq:---.-----";
#else
		pPrintStr = "Freq:---.-----";
#endif
	}

#ifdef ENABLE_CHINESE
	if (cn)
		UI_PrintStringSmallAtPixel(pPrintStr, 2, 2, 24u, 35u, 0u);
	else
#endif
		UI_PrintString(pPrintStr, 2, 0, 3, 8);

	/* 3rd line */
	if (gScanCssState < SCAN_CSS_STATE_FOUND) {
#ifdef ENABLE_CHINESE
		pPrintStr = cn ? "亚音:---" : "Tone:---";
#else
		pPrintStr = "Tone:---";
#endif
	} else if (!gScanUseCssResult) {
#ifdef ENABLE_CHINESE
		pPrintStr = cn ? "亚音:无" : "Tone:None";
#else
		pPrintStr = "Tone:None";
#endif
	} else if (gScanCssResultType == CODE_TYPE_CONTINUOUS_TONE) {
#ifdef ENABLE_CHINESE
		if (cn)
			sprintf(String, "模拟亚音:%u.%uHz",
				CTCSS_Options[gScanCssResultCode] / 10,
				CTCSS_Options[gScanCssResultCode] % 10);
		else
#endif
			sprintf(String, "CTCSS:%u.%uHz",
				CTCSS_Options[gScanCssResultCode] / 10,
				CTCSS_Options[gScanCssResultCode] % 10);
		pPrintStr = String;
	} else {
#ifdef ENABLE_CHINESE
		if (cn)
			sprintf(String, "数字亚音:D%03oN", DCS_Options[gScanCssResultCode]);
		else
#endif
			sprintf(String, "DCS:D%03oN", DCS_Options[gScanCssResultCode]);
		pPrintStr = String;
	}

#ifdef ENABLE_CHINESE
	if (cn)
		UI_PrintStringSmallAtPixel(pPrintStr, 2, 2, 40u, 51u, 0u);
	else
#endif
		UI_PrintString(pPrintStr, 2, 0, 5, 8);

	UI_BlitFullScreen();
}
