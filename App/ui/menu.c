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
#include <stdlib.h>

#include "../app/dtmf.h"
#include "../app/menu.h"
#include "../bitmaps.h"
#include "../board.h"
#include "../dcs.h"
#include "../driver/backlight.h"
#include "../driver/bk4819.h"
#include "../driver/eeprom.h"
#include "../driver/st7565.h"
#include "../external/printf/printf.h"
#include "../font.h"
#include "../frequencies.h"
#include "../helper/battery.h"
#include "../misc.h"
#include "../settings.h"

#ifdef ENABLE_FEAT_F4HWN
    #include "../version.h"
#endif

#include "../radio.h"
#include "helper.h"
#include "inputbox.h"
#include "menu.h"
#include "ui.h"
#include "welcome.h"

#ifdef ENABLE_CHINESE
#include "menu_sub_values_cn.h"
#define SUBV(en, cn) ((gUiLanguage == UI_LANGUAGE_CN) ? (const char *)(cn) : (const char *)(en))
#else
#define SUBV(en, cn) (en)
#endif


const t_menu_item MenuList[] =
{
//   text,          menu ID
#ifdef ENABLE_CHINESE
    {"Lang",        MENU_LANGUAGE      },
#endif
    {"Step",        MENU_STEP          },
    {"Power",       MENU_TXP           }, // was "TXP"
    {"RxDCS",       MENU_R_DCS         }, // was "R_DCS"
    {"RxCTCS",      MENU_R_CTCS        }, // was "R_CTCS"
    {"TxDCS",       MENU_T_DCS         }, // was "T_DCS"
    {"TxCTCS",      MENU_T_CTCS        }, // was "T_CTCS"
    {"TxODir",      MENU_SFT_D         }, // was "SFT_D"
    {"TxOffs",      MENU_OFFSET        }, // was "OFFSET"
    {"W/N",         MENU_W_N           },
#ifndef ENABLE_FEAT_F4HWN
    {"Scramb",      MENU_SCR           }, // was "SCR"
#endif
    {"BusyCL",      MENU_BCL           }, // was "BCL"
    {"Compnd",      MENU_COMPAND       },
    {"Mode",        MENU_AM            }, // was "AM"
#ifdef ENABLE_FEAT_F4HWN
    {"TXLock",      MENU_TX_LOCK       }, 
#endif
    {"ChList",      MENU_LIST_CH       },
    {"ChSave",      MENU_MEM_CH        }, // was "MEM-CH"
    {"ChDele",      MENU_DEL_CH        }, // was "DEL-CH"
    {"ChName",      MENU_MEM_NAME      },

    {"ScList",       MENU_S_LIST       },
    {"ScPri",        MENU_S_PRI        },
    {"PriCh1",       MENU_S_PRI_CH_1   },
    {"PriCh2",       MENU_S_PRI_CH_2   },
    {"ScnRev",      MENU_SC_REV        },
#ifndef ENABLE_FEAT_F4HWN
    #ifdef ENABLE_NOAA
        {"NOAA-S",      MENU_NOAA_S    },
    #endif
#endif
    {"F1Shrt",      MENU_F1SHRT        },
    {"F1Long",      MENU_F1LONG        },
    {"F2Shrt",      MENU_F2SHRT        },
    {"F2Long",      MENU_F2LONG        },
    {"M Long",      MENU_MLONG         },

    {"KeyLck",      MENU_AUTOLK        }, // was "AUTOLk"
    {"TxTOut",      MENU_TOT           }, // was "TOT"
    {"BatSav",      MENU_SAVE          }, // was "SAVE"
    {"BatTxt",      MENU_BAT_TXT       },
    {"Mic",         MENU_MIC           },
#ifdef ENABLE_AUDIO_BAR
    {"MicBar",      MENU_MIC_BAR       },
#endif
    // {"ChDisp",      MENU_MDF           }, // was "MDF" — hidden
    {"POnMsg",      MENU_PONMSG        },
    {"BLTime",      MENU_ABR           }, // was "ABR"
    {"BLMin",       MENU_ABR_MIN       },
    {"BLMax",       MENU_ABR_MAX       },
    {"BLTxRx",      MENU_ABR_ON_TX_RX  },
    {"Beep",        MENU_BEEP          },
#ifdef ENABLE_VOICE
    {"Voice",       MENU_VOICE         },
#endif
    {"Roger",       MENU_ROGER         },
    {"RogPrv",      MENU_ROGER_PREVIEW },
    {"Yan ID",      MENU_YAN_ID        },
    {"Yan Rx",      MENU_YAN_ID_RX     },
    {"STE",         MENU_STE           },
    {"RP STE",      MENU_RP_STE        },
    {"1 Call",      MENU_1_CALL        },
#ifdef ENABLE_ALARM
    {"AlarmT",      MENU_AL_MOD        },
#endif
#ifdef ENABLE_DTMF_CALLING
    {"ANI ID",      MENU_ANI_ID        },
#endif
    {"UPCode",      MENU_UPCODE        },
    {"DWCode",      MENU_DWCODE        },
    {"PTT ID",      MENU_PTT_ID        },
    {"D ST",        MENU_D_ST          },
#ifdef ENABLE_DTMF_CALLING
    {"D Resp",      MENU_D_RSP         },
    {"D Hold",      MENU_D_HOLD        },
#endif
    {"D Prel",      MENU_D_PRE         },
#ifdef ENABLE_DTMF_CALLING
    {"D Decd",      MENU_D_DCD         },
    {"D List",      MENU_D_LIST        },
#endif
    {"D Live",      MENU_D_LIVE_DEC    }, // live DTMF decoder
#ifndef ENABLE_FEAT_F4HWN
    #ifdef ENABLE_AM_FIX
        {"AM Fix",      MENU_AM_FIX        },
    #endif
#endif
    {"VOX",         MENU_VOX           },
#ifdef ENABLE_FEAT_F4HWN
    {"SysInf",      MENU_VOL           }, // was "VOL"
#else
    {"BatVol",      MENU_VOL           }, // was "VOL"
#endif
    {"RxMode",      MENU_TDR           },
    {"Sql",         MENU_SQL           },
#ifdef ENABLE_FEAT_F4HWN
    {"SetPwr",      MENU_SET_PWR       },
    {"SetPTT",      MENU_SET_PTT       },
    {"SetTOT",      MENU_SET_TOT       },
    {"SetEOT",      MENU_SET_EOT       },
    {"SetCtr",      MENU_SET_CTR       },
    {"SetInv",      MENU_SET_INV       },
    {"SetLck",      MENU_SET_LCK       },
    // {"SetMet",      MENU_SET_MET       }, // hidden
    // {"SetGUI",      MENU_SET_GUI       }, // hidden
#ifdef ENABLE_FEAT_F4HWN_AUDIO    
    {"SetRxA",      MENU_SET_AUD       },
#endif
    {"SetTmr",      MENU_SET_TMR       },
#ifdef ENABLE_FEAT_F4HWN_SLEEP
    {"SetOff",       MENU_SET_OFF      },
#endif
#ifdef ENABLE_FEAT_F4HWN_NARROWER
    {"SetNFM",      MENU_SET_NFM       },
#endif
#ifdef ENABLE_FEAT_F4HWN_VOL
    {"SetVol",      MENU_SET_VOL       },
#endif
#ifdef ENABLE_FEAT_F4HWN_RESCUE_OPS
    {"SetKey",      MENU_SET_KEY       },
#endif
#ifdef ENABLE_NOAA
    {"SetNWR",      MENU_NOAA_S    },
#endif
#ifdef ENABLE_FEAT_F4HWN_SCAN_FASTER
    {"SetScn",      MENU_SET_SCN       },
#endif
#ifdef ENABLE_FEAT_F4HWN_LOGO_SAV
    {"SetSav",      MENU_SET_SAV       },
#endif
#endif
    // hidden menu items from here on
    // enabled if pressing both the PTT and upper side button at power-on
    {"F Lock",      MENU_F_LOCK        },
#ifndef ENABLE_FEAT_F4HWN
    {"Tx 200",      MENU_200TX         }, // was "200TX"
    {"Tx 350",      MENU_350TX         }, // was "350TX"
    {"Tx 500",      MENU_500TX         }, // was "500TX"
#endif
    {"350 En",      MENU_350EN         }, // was "350EN"
#ifndef ENABLE_FEAT_F4HWN
    {"ScraEn",      MENU_SCREN         }, // was "SCREN"
#endif
#ifdef ENABLE_F_CAL_MENU
    {"FrCali",      MENU_F_CALI        }, // reference xtal calibration
#endif
    {"BatCal",      MENU_BATCAL        }, // battery voltage calibration
    {"BatTyp",      MENU_BATTYP        }, // battery type 1600/2200mAh
    {"SetNav",      MENU_SET_NAV       }, // set navigation (LEFT / RIGHT or UP / DOWN)
    {"Reset",       MENU_RESET         }, // might be better to move this to the hidden menu items ?

    {"",                              0xff               }  // end of list - DO NOT delete or move this this
};

const uint8_t FIRST_HIDDEN_MENU_ITEM = MENU_F_LOCK;

const char* const gSubMenu_TXP[] =
{
    "USER",
    "LOW 1",
    "LOW 2",
    "LOW 3",
    "LOW 4",
    "LOW 5",
    "MID",
    "HIGH"
};

const char* const gSubMenu_SFT_D[] =
{
    "OFF",
    "+",
    "-"
};

const char* const gSubMenu_W_N[] =
{
    "WIDE",
    "NARROW"
};

const char* const gSubMenu_OFF_ON[] =
{
    "OFF",
    "ON"
};

const char* gSubMenu_NA = "N/A";

const char* const gSubMenu_RXMode[] =
{
    "MAIN\nONLY",       // TX and RX on main only
    "DUAL RX\nRESPOND", // Watch both and respond
    "CROSS\nBAND",      // TX on main, RX on secondary
    "MAIN TX\nDUAL RX", // always TX on main, but RX on both
    "TRI\nWATCH"        // watch three channels
};

#ifdef ENABLE_VOICE
    const char* const gSubMenu_VOICE[] =
    {
        "OFF",
        "CHI",
        "ENG"
    };
#endif

const char* const gSubMenu_MDF[] =
{
    "FREQ",
    "CHANNEL\nNUMBER",
    "NAME",
    "NAME\n+\nFREQ"
};

#ifdef ENABLE_ALARM
    const char* const gSubMenu_AL_MOD[] =
    {
        "SITE",
        "TONE"
    };
#endif

#ifdef ENABLE_DTMF_CALLING
const char* const gSubMenu_D_RSP[] =
{
    "DO\nNOTHING",
    "RING",
    "REPLY",
    "BOTH"
};
#endif

const char* const gSubMenu_PTT_ID[] =
{
    "OFF",
    "UP CODE",
    "DOWN CODE",
    "UP+DOWN\nCODE",
    "APOLLO\nQUINDAR"
};

const char* const gSubMenu_PONMSG[] =
{
#ifdef ENABLE_FEAT_F4HWN
    "NONE",
    "DEFAULT",
#ifdef ENABLE_FEAT_F4HWN_LOGO
    "LOGO",
#endif
#else
    "FULL",
    "MESSAGE",
    "VOLTAGE",
    "NONE"
#endif
};

#if defined(ENABLE_FEAT_F4HWN) && defined(ENABLE_FEAT_F4HWN_LOGO_SAV)
const char* const gSubMenu_SET_SAV[] =
{
    "OFF",
    "LOGO",
    "LOGO+",
    "MATRIX"
};
#endif

const char* const gSubMenu_ROGER[] =
{
    "OFF",
    "ROGER",
    "MDC",
    "Yan ID",
    "Custom 1",
    "Custom 2",
    "Custom 3"
};

const char* const gSubMenu_RESET[] =
{
    "VFO",
    "ALL"
};

const char* const gSubMenu_F_LOCK[] =
{
    "DEFAULT+\n137-174\n400-470",
    "FCC HAM\n144-148\n420-450",
#ifdef ENABLE_FEAT_F4HWN_CA
    "CA HAM\n144-148\n430-450",
#endif
    "CE HAM\n144-146\n430-440",
    "GB HAM\n144-148\n430-440",
    "137-174\n400-430",
    "137-174\n400-438",
#ifdef ENABLE_FEAT_F4HWN_PMR
    "PMR 446",
#endif
#ifdef ENABLE_FEAT_F4HWN_GMRS_FRS_MURS
    "GMRS\nFRS\nMURS",
#endif
    "DISABLE\nALL",
    "UNLOCK\nALL",
};

const char* const gSubMenu_RX_TX[] =
{
    "OFF",
    "TX",
    "RX",
    "TX/RX"
};

const char* const gSubMenu_BAT_TXT[] =
{
    "NONE",
    "VOLTAGE",
    "PERCENT"
};

const char gSubMenu_LANGUAGE[][8] =
{
    "English",
    "Chinese"
};

#ifdef ENABLE_CHINESE
/* Popup only — list preview forces English via GetItemPreview */
static const char gSubMenu_LANGUAGE_CN[][8] =
{
    "\xe8\x8b\xb1\xe8\xaf\xad", /* 英语 */
    "\xe4\xb8\xad\xe6\x96\x87"  /* 中文 */
};
#endif

const char* const gSubMenu_BATTYP[] =
{
    "1600mAh K5",
    "2200mAh K5",
    "3500mAh K5",
    "1400mAh K1",
    "2500mAh K1"
};

const char* const gSubMenu_SET_NAV[] =
{
    "LEFT\nRIGHT\nUV-K1",
    "UP\nDOWN\nUV-K5(8)",
};

#ifndef ENABLE_FEAT_F4HWN
const char* const gSubMenu_SCRAMBLER[] =
{
    "OFF",
    "2600Hz",
    "2700Hz",
    "2800Hz",
    "2900Hz",
    "3000Hz",
    "3100Hz",
    "3200Hz",
    "3300Hz",
    "3400Hz",
    "3500Hz"
};
#endif

#ifdef ENABLE_FEAT_F4HWN
    const char* const gSubMenu_SET_PWR[] =
    {
        "< 20m",
        "125m",
        "250m",
        "500m",
        "1",
        "2",
        "5"
    };

    const char* const gSubMenu_SET_PTT[] =
    {
        "CLASSIC",
        "ONEPUSH"
    };

    const char* const gSubMenu_SET_TOT[] =  
    {
        "OFF",
        "SOUND",
        "VISUAL",
        "ALL"
    };

    const char* const gSubMenu_SET_LCK[] =
    {
        "KEYS",
        "KEYS\nACTIONS",
        "KEYS\nPTT",
        "KEYS\nACTIONS\nPTT"
    };

    const char* const gSubMenu_SET_MET[] =
    {
        "TINY",
        "CLASSIC"
    };

    #ifdef ENABLE_FEAT_F4HWN_SCAN_FASTER
        const char* const gSubMenu_SET_SCN[] =
        {
            "NORMAL",
            "FAST"
        };
    #endif

    #ifdef ENABLE_FEAT_F4HWN_AUDIO
        const char* const gSubMenu_SET_AUD_FM[] =
        {
            "FLAT",
            "CLEAN",
            "MID",
            "BOOST",
            "MAX"
        };

        const char* const gSubMenu_SET_AUD_AM[] =
        {
            "SHARP",
            "STOCK",
            "OPEN"
        };
    #endif

    #ifdef ENABLE_FEAT_F4HWN_NARROWER
        const char* const gSubMenu_SET_NFM[] =
        {
            "NARROW",
            "NARROWER"
        };
    #endif

    #ifdef ENABLE_FEAT_F4HWN_RESCUE_OPS
        const char* const gSubMenu_SET_KEY[] =
        {
            "KEY_MENU",
            "KEY_UP",
            "KEY_DOWN",
            "KEY_EXIT",
            "KEY_STAR"
        };
    #endif
#endif

const t_sidefunction gSubMenu_SIDEFUNCTIONS[] =
{
    {"NONE",            ACTION_OPT_NONE},
#ifdef ENABLE_FLASHLIGHT
    {"FLASH\nLIGHT",    ACTION_OPT_FLASHLIGHT},
#endif
    {"POWER",           ACTION_OPT_POWER},
    {"MONITOR",         ACTION_OPT_MONITOR},
    {"SCAN",            ACTION_OPT_SCAN},
#ifdef ENABLE_VOX
    {"VOX",             ACTION_OPT_VOX},
#endif
#ifdef ENABLE_ALARM
    {"ALARM",           ACTION_OPT_ALARM},
#endif
#ifdef ENABLE_FMRADIO
    {"FM RADIO",        ACTION_OPT_FM},
#endif
#ifdef ENABLE_TX1750
    {"1750Hz",          ACTION_OPT_1750},
#endif
    {"LOCK\nKEYPAD",    ACTION_OPT_KEYLOCK},
    {"VFO A\nVFO B",    ACTION_OPT_A_B},
    {"VFO\nMEM",        ACTION_OPT_VFO_MR},
    {"MODE",            ACTION_OPT_SWITCH_DEMODUL},
#ifdef ENABLE_BLMIN_TMP_OFF
    {"BLMIN\nTMP OFF",  ACTION_OPT_BLMIN_TMP_OFF},      //BackLight Minimum Temporary OFF
#endif
#ifdef ENABLE_FEAT_F4HWN
    {"RX MODE",         ACTION_OPT_RXMODE},
    {"MAIN ONLY",       ACTION_OPT_MAINONLY},
    {"PTT",             ACTION_OPT_PTT},
    {"WIDE\nNARROW",    ACTION_OPT_WN},
    {"MUTE",            ACTION_OPT_MUTE},
    #ifdef ENABLE_FEAT_F4HWN_AUDIO
        {"RxA",            ACTION_OPT_RXA},
    #endif
    #ifdef ENABLE_FEAT_F4HWN_RESCUE_OPS
        {"POWER\nHIGH",    ACTION_OPT_POWER_HIGH},
        {"REMOVE\nOFFSET",  ACTION_OPT_REMOVE_OFFSET},
    #endif
    #ifdef ENABLE_FEAT_F4HWN_BEAM
        {"BEAM",            ACTION_OPT_BEAM},
    #endif
    #ifdef ENABLE_FEAT_F4HWN_RXTX_LOG
        {"RF LOG",          ACTION_OPT_RXTX_LOG},
    #endif
    #ifdef ENABLE_FEAT_F4HWN_FOXHUNT
        {"FOX HUNT\nBEACON", ACTION_OPT_FOXHUNT},
    #endif
#endif
#ifdef ENABLE_AIRCRAFT_RADAR
    {"AIRCRAFT\nRADAR", ACTION_OPT_AIRCRAFT},
#endif
#ifdef ENABLE_APRS
    {"APRS",            ACTION_OPT_APRS},
#endif
};

const uint8_t gSubMenu_SIDEFUNCTIONS_size = ARRAY_SIZE(gSubMenu_SIDEFUNCTIONS);

bool    gIsInSubMenu;
uint8_t gMenuCursor;
uint8_t gMenuIndices[ARRAY_SIZE(MenuList)]; // Etape 1: table position affichee -> index MenuList (vue courante)

int UI_MENU_GetCurrentMenuId() {
    if(gMenuCursor < gMenuListCount)
        return MenuList[gMenuIndices[gMenuCursor]].menu_id;

    return MenuList[ARRAY_SIZE(MenuList)-1].menu_id;
}

uint8_t UI_MENU_GetMenuIdx(uint8_t id)
{
    for(uint8_t i = 0; i < ARRAY_SIZE(MenuList); i++)
        if(MenuList[i].menu_id == id)
            return i;
    return 0;
}

// Position dans la vue courante (gMenuIndices) du menu_id, ou gMenuCursor si absent.
// En vue All (identite) equivaut a UI_MENU_GetMenuIdx ; en vue categorie, donne la
// position filtree correcte.
uint8_t UI_MENU_GetViewPos(uint8_t id)
{
    for (uint8_t i = 0; i < gMenuListCount; i++)
        if (MenuList[gMenuIndices[i]].menu_id == id)
            return i;
    return gMenuCursor;
}

static bool UI_MENU_ItemHidden(uint8_t menu_id)
{
    if (menu_id == MENU_YAN_ID_RX && gEeprom.ROGER != ROGER_MODE_YAN_ID)
        return true;
    if (gEeprom.TRIPLE_WATCH &&
        (menu_id == MENU_F1SHRT || menu_id == MENU_F1LONG ||
         menu_id == MENU_F2SHRT || menu_id == MENU_F2LONG))
        return true;
    return false;
}

#ifdef ENABLE_FEAT_F4HWN_MENU_CAT
// --- Etape 2a : donnee du classement par categorie (cible Fusion) ---
// Chaque liste = les menu_id d'une categorie, DANS l'ordre d'affichage voulu
// (ex. SetPwr colle a Power). CAT_ALL n'a pas de liste : il reprend MenuList
// tel quel, donc ordre et numeros d'origine preserves.
const char *const CategoryNames[CAT_COUNT] = {
    [CAT_CHANNELS] = "Channels",
    [CAT_SCAN]     = "Scan",
    [CAT_KEYS]     = "Keys",
    [CAT_POWER]    = "Power",
    [CAT_DISPLAY]  = "Display",
    [CAT_TIMERS]   = "Timers",
    [CAT_AUDIO]    = "Audio",
    [CAT_RADIO]    = "Radio",
    [CAT_DTMF]     = "DTMF",
    [CAT_SERVICE]  = "Service",
    [CAT_ALL]      = "All",
};

// Les menu_id de sous-features optionnelles sont gardes exactement comme dans
// l'enum (menu.h) : sur un preset qui ne les compile pas, ils ne sont pas
// references (sinon build KO, ex. preset Custom). Les autres MENU_SET_* sont
// sous ENABLE_FEAT_F4HWN, garanti par la dependance CMake (App/CMakeLists.txt).
static const uint8_t CatChannels[] = {
    MENU_STEP, MENU_TXP, MENU_SET_PWR, MENU_R_DCS, MENU_R_CTCS, MENU_T_DCS,
    MENU_T_CTCS, MENU_SFT_D, MENU_OFFSET, MENU_W_N,
#ifdef ENABLE_FEAT_F4HWN_NARROWER
    MENU_SET_NFM,
#endif
    MENU_BCL, MENU_COMPAND, MENU_AM, MENU_TX_LOCK, MENU_PTT_ID, MENU_LIST_CH,
    MENU_MEM_CH, MENU_DEL_CH, MENU_MEM_NAME,
};
static const uint8_t CatScan[]    = {
    MENU_S_LIST, MENU_S_PRI, MENU_S_PRI_CH_1, MENU_S_PRI_CH_2, MENU_SC_REV,
#ifdef ENABLE_FEAT_F4HWN_SCAN_FASTER
    MENU_SET_SCN,
#endif
};
static const uint8_t CatKeys[]    = {
    MENU_F1SHRT, MENU_F1LONG, MENU_F2SHRT, MENU_F2LONG, MENU_MLONG, MENU_AUTOLK, MENU_SET_LCK,
#ifdef ENABLE_FEAT_F4HWN_RESCUE_OPS
    MENU_SET_KEY,
#endif
    MENU_SET_PTT, MENU_1_CALL,
};
static const uint8_t CatPower[]   = {
    MENU_SAVE, MENU_BAT_TXT,
#ifdef ENABLE_FEAT_F4HWN_SLEEP
    MENU_SET_OFF,
#endif
#ifdef ENABLE_FEAT_F4HWN_LOGO_SAV
    MENU_SET_SAV,
#endif
};
static const uint8_t CatDisplay[] = { MENU_LANGUAGE, MENU_ABR, MENU_ABR_MIN, MENU_ABR_MAX, MENU_ABR_ON_TX_RX, MENU_SET_CTR, MENU_SET_INV, MENU_VOL };
static const uint8_t CatTimers[]  = { MENU_TOT, MENU_SET_TOT, MENU_SET_EOT, MENU_SET_TMR };
static const uint8_t CatAudio[]   = {
    MENU_MIC,
#ifdef ENABLE_AUDIO_BAR
    MENU_MIC_BAR,
#endif
    MENU_BEEP,
#ifdef ENABLE_FEAT_F4HWN_VOL
    MENU_SET_VOL,
#endif
#ifdef ENABLE_FEAT_F4HWN_AUDIO
    MENU_SET_AUD,
#endif
};
static const uint8_t CatRadio[]   = { MENU_SQL, MENU_STE, MENU_RP_STE, MENU_ROGER, MENU_ROGER_PREVIEW, MENU_YAN_ID, MENU_YAN_ID_RX, MENU_VOX, MENU_TDR };
static const uint8_t CatDtmf[]    = { MENU_UPCODE, MENU_DWCODE, MENU_D_ST, MENU_D_PRE, MENU_D_LIVE_DEC };
static const uint8_t CatService[] = { MENU_F_LOCK, MENU_350EN, MENU_BATCAL, MENU_BATTYP, MENU_SET_NAV, MENU_RESET };

typedef struct { const uint8_t *ids; uint8_t len; } cat_list_t;

static const cat_list_t CategoryLists[CAT_COUNT] = {
    [CAT_CHANNELS] = { CatChannels, ARRAY_SIZE(CatChannels) },
    [CAT_SCAN]     = { CatScan,     ARRAY_SIZE(CatScan)     },
    [CAT_KEYS]     = { CatKeys,     ARRAY_SIZE(CatKeys)     },
    [CAT_POWER]    = { CatPower,    ARRAY_SIZE(CatPower)    },
    [CAT_DISPLAY]  = { CatDisplay,  ARRAY_SIZE(CatDisplay)  },
    [CAT_TIMERS]   = { CatTimers,   ARRAY_SIZE(CatTimers)   },
    [CAT_AUDIO]    = { CatAudio,    ARRAY_SIZE(CatAudio)    },
    [CAT_RADIO]    = { CatRadio,    ARRAY_SIZE(CatRadio)    },
    [CAT_DTMF]     = { CatDtmf,     ARRAY_SIZE(CatDtmf)     },
    [CAT_SERVICE]  = { CatService,  ARRAY_SIZE(CatService)  },
    [CAT_ALL]      = { NULL, 0 },
};

uint8_t gMenuCategory = CAT_ALL;

// Index de 'id' dans MenuList, ou 0xFF si absent (item non compile).
static uint8_t menu_find_idx(uint8_t id)
{
    for (uint8_t i = 0; MenuList[i].name[0] != '\0'; i++)
        if (MenuList[i].menu_id == id)
            return i;
    return 0xFF;
}

uint8_t gMenuLevel     = MENU_LEVEL_CAT;
uint8_t gCatOrder[CAT_COUNT];
uint8_t gMenuCatCursor = 0;
uint8_t gCatLastPos[CAT_COUNT];   // derniere position du curseur item, par categorie

// Nombre d'items presents (compiles) dans une categorie.
uint8_t UI_MENU_CategoryItemCount(uint8_t cat)
{
    uint8_t n = 0;

    if (cat == CAT_ALL)
    {
        for (uint8_t i = 0; MenuList[i].name[0] != '\0'; i++)
        {
            if (!gF_LOCK && MenuList[i].menu_id == FIRST_HIDDEN_MENU_ITEM)
                break;
            if (UI_MENU_ItemHidden(MenuList[i].menu_id))
                continue;
            n++;
        }
        return n;
    }

    const cat_list_t *cl = &CategoryLists[cat];
    for (uint8_t k = 0; k < cl->len; k++) {
        if (UI_MENU_ItemHidden(cl->ids[k]))
            continue;
        if (menu_find_idx(cl->ids[k]) != 0xFF)
            n++;
    }
    return n;
}

// Ecran d'entree : liste plate ALL (gMenuIndices + gMenuListCount).
void UI_MENU_BuildCategoryScreen(void)
{
    gMenuCategory  = CAT_ALL;
    UI_MENU_BuildView();
}
#endif

// Dessine le texte small a une ordonnee pixel (pas seulement une ligne fb).
static void UI_MENU_DrawSmallAtY(const char *text, uint8_t x, uint8_t y_top)
{
    const uint8_t char_w = (uint8_t)ARRAY_SIZE(gFontSmall[0]);
    const uint8_t pitch  = (uint8_t)(char_w + 1u);
    const uint8_t y_max  = (uint8_t)(FRAME_LINES * 8u);

    for (; *text != '\0'; text++)
    {
        if (*text > ' ' && *text < 127)
        {
            const uint8_t *glyph = gFontSmall[*text - ' ' - 1];
            const uint8_t  gx    = (uint8_t)(x + 1u);

            for (uint8_t col = 0; col < char_w; col++)
            {
                const uint8_t bits = glyph[col];
                for (uint8_t row = 0; row < 8u; row++)
                {
                    const uint8_t py = (uint8_t)(y_top + row);
                    if (py >= y_max)
                        break;
                    if (bits & (uint8_t)(1u << row))
                        UI_DrawPixelBuffer(gFrameBuffer, (uint8_t)(gx + col), py, true);
                }
            }
        }
        x = (uint8_t)(x + pitch);
    }
}

static void UI_MENU_XorBand(uint8_t y0, uint8_t height)
{
    const uint8_t y_max = (uint8_t)(FRAME_LINES * 8u);

    for (uint8_t dy = 0; dy < height; dy++)
    {
        const uint8_t y = (uint8_t)(y0 + dy);
        uint8_t       mask;

        if (y >= y_max)
            break;

        mask = (uint8_t)(1u << (y & 7u));
        for (uint8_t x = 0; x < LCD_WIDTH; x++)
            gFrameBuffer[y >> 3][x] ^= mask;
    }
}

static void UI_MENU_FirstLine(char *dst, const char *src, uint8_t max_len)
{
	uint8_t i = 0;

	if (max_len == 0)
		return;
	while (src[i] != '\0' && src[i] != '\n' && (uint8_t)(i + 1u) < max_len)
	{
		dst[i] = src[i];
		i++;
	}
	dst[i] = '\0';
}

// Formate la valeur courante (gSubMenuSelection) — partage liste + popup.
static void UI_MENU_FormatDtmfCode(char *out, const char *code)
{
	if (code[8] != '\0' && (uint8_t)code[8] != 0xFFu)
		sprintf(out, "%.8s\n%.8s", code, code + 8);
	else
		sprintf(out, "%.8s", code);
}

static void UI_MENU_FormatValue(const int m, char *out, uint8_t out_len)
{
	if (out_len == 0)
		return;
	out[0] = '\0';

	switch (m)
	{
		case MENU_SQL:
			sprintf(out, "%d", (int)gSubMenuSelection);
			break;
		case MENU_MIC: {
			const uint8_t mic = gMicGain_dB2[gSubMenuSelection];
			sprintf(out, "+%u.%udB", mic / 2, (mic % 2) * 5);
			break;
		}
		case MENU_STEP: {
			uint16_t step = gStepFrequencyTable[FREQUENCY_GetStepIdxFromSortedIdx(gSubMenuSelection)];
			sprintf(out, "%d.%02ukHz", step / 100, step % 100);
			break;
		}
		case MENU_TXP:
			if (gSubMenuSelection == 0)
				strcpy(out, SUBV(gSubMenu_TXP[0], gSubMenu_TXP_CN[0]));
			else
#ifdef ENABLE_FEAT_F4HWN
				sprintf(out, "%s\n%sW",
					SUBV(gSubMenu_TXP[gSubMenuSelection], gSubMenu_TXP_CN[gSubMenuSelection]),
					SUBV(gSubMenu_SET_PWR[gSubMenuSelection - 1], gSubMenu_SET_PWR_CN[gSubMenuSelection - 1]));
#else
				strcpy(out, SUBV(gSubMenu_TXP[gSubMenuSelection], gSubMenu_TXP_CN[gSubMenuSelection]));
#endif
			break;
		case MENU_R_DCS:
		case MENU_T_DCS:
			if (gSubMenuSelection == 0)
				strcpy(out, SUBV(gSubMenu_OFF_ON[0], gSubMenu_OFF_ON_CN[0]));
			else if (gSubMenuSelection < 105)
				sprintf(out, "D%03oN", DCS_Options[gSubMenuSelection - 1]);
			else
				sprintf(out, "D%03oI", DCS_Options[gSubMenuSelection - 105]);
			break;
		case MENU_R_CTCS:
		case MENU_T_CTCS:
			if (gSubMenuSelection == 0)
				strcpy(out, SUBV(gSubMenu_OFF_ON[0], gSubMenu_OFF_ON_CN[0]));
			else
				sprintf(out, "%u.%uHz", CTCSS_Options[gSubMenuSelection - 1] / 10, CTCSS_Options[gSubMenuSelection - 1] % 10);
			break;
		case MENU_SFT_D:
			strcpy(out, SUBV(gSubMenu_SFT_D[gSubMenuSelection], gSubMenu_SFT_D_CN[gSubMenuSelection]));
			break;
		case MENU_OFFSET:
			sprintf(out, "%d.%05u", (int)(gSubMenuSelection / 100000), (unsigned)(abs(gSubMenuSelection) % 100000));
			break;
		case MENU_W_N:
			strcpy(out, SUBV(gSubMenu_W_N[gSubMenuSelection], gSubMenu_W_N_CN[gSubMenuSelection]));
			break;
		case MENU_AM:
			strcpy(out, SUBV(gModulationStr[gSubMenuSelection], gSubMenu_MODULATION_CN[gSubMenuSelection]));
			break;
		case MENU_MEM_NAME:
			SETTINGS_FetchChannelName(out, (uint16_t)gSubMenuSelection);
			if (out[0] == '\0')
				strcpy(out, "--");
#ifdef ENABLE_CHINESE
			else if (SETTINGS_ChannelNameHasCjkUtf8(out))
				/* List 3x5 cannot draw Hanzi — same as ChSave/ChDele: show channel number */
				UI_GenerateChannelStringEx(out, true, (uint16_t)gSubMenuSelection);
#endif
			break;
		case MENU_MEM_CH:
		case MENU_DEL_CH:
		case MENU_1_CALL:
		case MENU_S_PRI_CH_1:
		case MENU_S_PRI_CH_2:
			if (gSubMenuSelection == MR_CHANNELS_MAX)
				strcpy(out, SUBV("None", gSubMenu_MEM_NONE_CN));
			else
				UI_GenerateChannelStringEx(out, true, (uint16_t)gSubMenuSelection);
			break;
		case MENU_BCL:
		case MENU_BEEP:
		case MENU_STE:
		case MENU_D_ST:
		case MENU_D_LIVE_DEC:
		case MENU_350EN:
#ifndef ENABLE_FEAT_F4HWN
		case MENU_350TX:
		case MENU_200TX:
		case MENU_500TX:
		case MENU_SCREN:
#ifdef ENABLE_AM_FIX
		case MENU_AM_FIX:
#endif
#endif
#ifdef ENABLE_DTMF_CALLING
		case MENU_D_DCD:
#endif
#ifdef ENABLE_NOAA
		case MENU_NOAA_S:
#endif
#ifdef ENABLE_FEAT_F4HWN
		case MENU_SET_TMR:
		case MENU_S_PRI:
#ifdef ENABLE_FEAT_F4HWN_INV
		case MENU_SET_INV:
#endif
#endif
			strcpy(out, SUBV(gSubMenu_OFF_ON[gSubMenuSelection], gSubMenu_OFF_ON_CN[gSubMenuSelection]));
			break;
		case MENU_MIC_BAR:
#ifdef ENABLE_AUDIO_BAR
			strcpy(out, SUBV(gSubMenu_OFF_ON[gSubMenuSelection], gSubMenu_OFF_ON_CN[gSubMenuSelection]));
#else
			strcpy(out, gSubMenu_NA);
#endif
			break;
		case MENU_SAVE:
			if (gSubMenuSelection == 0)
				strcpy(out, SUBV(gSubMenu_OFF_ON[0], gSubMenu_OFF_ON_CN[0]));
			else
				sprintf(out, "1:%u", (unsigned)gSubMenuSelection);
			break;
		case MENU_TDR:
			strcpy(out, SUBV(gSubMenu_RXMode[gSubMenuSelection], gSubMenu_RXMode_CN[gSubMenuSelection]));
			break;
		case MENU_TOT:
			sprintf(out, "%02dm:%02ds", (int)((((gSubMenuSelection + 1) * 5) / 60)), (int)((((gSubMenuSelection + 1) * 5) % 60)));
			break;
		case MENU_ABR:
			if (gSubMenuSelection == 0)
				strcpy(out, SUBV(gSubMenu_OFF_ON[0], gSubMenu_OFF_ON_CN[0]));
			else if (gSubMenuSelection < 61)
				sprintf(out, "%02dm:%02ds", (int)((gSubMenuSelection * 5) / 60), (int)((gSubMenuSelection * 5) % 60));
			else
				strcpy(out, SUBV("ON", gSubMenu_ABR_ON_CN));
			break;
		case MENU_ABR_MIN:
		case MENU_ABR_MAX:
#if defined(ENABLE_FEAT_F4HWN) && defined(ENABLE_FEAT_F4HWN_CTR)
		case MENU_SET_CTR:
#endif
			sprintf(out, "%d", (int)gSubMenuSelection);
			break;
		case MENU_BAT_TXT:
			strcpy(out, SUBV(gSubMenu_BAT_TXT[gSubMenuSelection], gSubMenu_BAT_TXT_CN[gSubMenuSelection]));
			break;
		case MENU_PONMSG:
			strcpy(out, SUBV(gSubMenu_PONMSG[gSubMenuSelection], gSubMenu_PONMSG_CN[gSubMenuSelection]));
			break;
		case MENU_ROGER:
		case MENU_ROGER_PREVIEW:
			strcpy(out, SUBV(gSubMenu_ROGER[gSubMenuSelection], gSubMenu_ROGER_CN[gSubMenuSelection]));
			break;
		case MENU_YAN_ID:
			if (gEeprom.yan_id[0])
				strncpy(out, gEeprom.yan_id, out_len - 1u);
			else
				strcpy(out, "--");
			out[out_len - 1u] = 0;
			break;
		case MENU_YAN_ID_RX:
			strcpy(out, SUBV(gSubMenu_OFF_ON[gSubMenuSelection], gSubMenu_OFF_ON_CN[gSubMenuSelection]));
			break;
		case MENU_PTT_ID:
			strcpy(out, SUBV(gSubMenu_PTT_ID[gSubMenuSelection], gSubMenu_PTT_ID_CN[gSubMenuSelection]));
			break;
		case MENU_MDF:
			strcpy(out, SUBV(gSubMenu_MDF[gSubMenuSelection], gSubMenu_MDF_CN[gSubMenuSelection]));
			break;
#ifdef ENABLE_VOICE
		case MENU_VOICE:
			strcpy(out, SUBV(gSubMenu_VOICE[gSubMenuSelection], gSubMenu_VOICE_CN[gSubMenuSelection]));
			break;
#endif
#ifdef ENABLE_ALARM
		case MENU_AL_MOD:
			strcpy(out, SUBV(gSubMenu_AL_MOD[gSubMenuSelection], gSubMenu_AL_MOD_CN[gSubMenuSelection]));
			break;
#endif
		case MENU_F_LOCK:
#ifdef ENABLE_FEAT_F4HWN
			if (!gIsInSubMenu && gUnlockAllTxConfCnt > 0 && gUnlockAllTxConfCnt < 3)
#else
			if (!gIsInSubMenu && gUnlockAllTxConfCnt > 0 && gUnlockAllTxConfCnt < 10)
#endif
				strcpy(out, "READ\nMANUAL");
			else
				strcpy(out, SUBV(gSubMenu_F_LOCK[gSubMenuSelection], gSubMenu_F_LOCK_CN[gSubMenuSelection]));
			break;
		case MENU_RESET:
			strcpy(out, SUBV(gSubMenu_RESET[gSubMenuSelection], gSubMenu_RESET_CN[gSubMenuSelection]));
			break;
		case MENU_BATTYP:
			strcpy(out, gSubMenu_BATTYP[gSubMenuSelection]);
			break;
		case MENU_SET_NAV:
			strcpy(out, SUBV(gSubMenu_SET_NAV[gSubMenuSelection], gSubMenu_SET_NAV_CN[gSubMenuSelection]));
			break;
		case MENU_LANGUAGE:
			strcpy(out, SUBV(gSubMenu_LANGUAGE[gSubMenuSelection],
				gSubMenu_LANGUAGE_CN[gSubMenuSelection]));
			break;
		case MENU_F1SHRT:
		case MENU_F1LONG:
		case MENU_F2SHRT:
		case MENU_F2LONG:
		case MENU_MLONG:
			strcpy(out, SUBV(gSubMenu_SIDEFUNCTIONS[gSubMenuSelection].name, gSubMenu_SIDEFUNCTIONS_CN[gSubMenuSelection]));
			break;
		case MENU_LIST_CH:
		case MENU_S_LIST:
			if (gSubMenuSelection == MR_CHANNELS_LIST + 1)
				strcpy(out, SUBV("ALL", gSubMenu_LIST_CN_ALL));
			else if (gSubMenuSelection == 0 && m == MENU_LIST_CH)
				strcpy(out, SUBV("OFF", gSubMenu_LIST_CN_OFF));
			else {
				const char *name = gListName[gSubMenuSelection - 1];
				if (IsEmptyName(name, sizeof(gListName[0])))
					sprintf(out, "%02u", (unsigned)gSubMenuSelection);
				else
					sprintf(out, "%02u (%.3s)", (unsigned)gSubMenuSelection, name);
			}
			break;
		case MENU_VOX:
#ifdef ENABLE_VOX
			if (gSubMenuSelection == 0)
				strcpy(out, SUBV(gSubMenu_OFF_ON[0], gSubMenu_OFF_ON_CN[0]));
			else
				sprintf(out, "%u", (unsigned)gSubMenuSelection);
#else
			strcpy(out, gSubMenu_NA);
#endif
			break;
		case MENU_COMPAND:
		case MENU_ABR_ON_TX_RX:
			strcpy(out, SUBV(gSubMenu_RX_TX[gSubMenuSelection], gSubMenu_RX_TX_CN[gSubMenuSelection]));
			break;
		case MENU_SC_REV:
			if (gSubMenuSelection == 0)
				strcpy(out, SUBV("STOP", gSubMenu_SC_REV_STOP_CN));
			else if (gSubMenuSelection < 81)
				sprintf(out, "CARRIER\n%02ds:%03dms", (int)((gSubMenuSelection * 250) / 1000), (int)((gSubMenuSelection * 250) % 1000));
			else
				sprintf(out, "TIMEOUT\n%02dm:%02ds", (int)(((gSubMenuSelection - 80) * 5) / 60), (int)(((gSubMenuSelection - 80) * 5) % 60));
			break;
		case MENU_AUTOLK:
			if (gSubMenuSelection == 0)
				strcpy(out, SUBV(gSubMenu_OFF_ON[0], gSubMenu_OFF_ON_CN[0]));
			else
				sprintf(out, "%02dm:%02ds", (int)((gSubMenuSelection * 15) / 60), (int)((gSubMenuSelection * 15) % 60));
			break;
#ifdef ENABLE_DTMF_CALLING
		case MENU_ANI_ID:
			strcpy(out, gEeprom.ANI_DTMF_ID);
			break;
		case MENU_D_RSP:
			strcpy(out, SUBV(gSubMenu_D_RSP[gSubMenuSelection], gSubMenu_D_RSP_CN[gSubMenuSelection]));
			break;
		case MENU_D_HOLD:
			sprintf(out, "%ds", (int)gSubMenuSelection);
			break;
#endif
		case MENU_UPCODE:
			UI_MENU_FormatDtmfCode(out, gEeprom.DTMF_UP_CODE);
			break;
		case MENU_DWCODE:
			UI_MENU_FormatDtmfCode(out, gEeprom.DTMF_DOWN_CODE);
			break;
		case MENU_D_PRE:
			sprintf(out, "%d*10ms", (int)gSubMenuSelection);
			break;
		case MENU_RP_STE:
			if (gSubMenuSelection == 0)
				strcpy(out, SUBV(gSubMenu_OFF_ON[0], gSubMenu_OFF_ON_CN[0]));
			else
				sprintf(out, "%u*100ms", (unsigned)gSubMenuSelection);
			break;
		case MENU_BATCAL: {
			const uint16_t vol = (uint32_t)gBatteryVoltageAverage * gBatteryCalibration[3] / gSubMenuSelection;
			sprintf(out, "%u.%02uV\n%u", vol / 100, vol % 100, (unsigned)gSubMenuSelection);
			break;
		}
#ifndef ENABLE_FEAT_F4HWN
		case MENU_SCR:
			strcpy(out, SUBV(gSubMenu_SCRAMBLER[gSubMenuSelection], gSubMenu_SCRAMBLER_CN[gSubMenuSelection]));
			break;
#endif
#if defined(ENABLE_FEAT_F4HWN) && defined(ENABLE_FEAT_F4HWN_LOGO_SAV)
		case MENU_SET_SAV:
			strcpy(out, SUBV(gSubMenu_SET_SAV[gSubMenuSelection], gSubMenu_SET_SAV_CN[gSubMenuSelection]));
			break;
#endif
#ifdef ENABLE_FEAT_F4HWN
		case MENU_SET_PWR:
			sprintf(out, "%s\n%sW",
				SUBV(gSubMenu_TXP[gSubMenuSelection + 1], gSubMenu_TXP_CN[gSubMenuSelection + 1]),
				SUBV(gSubMenu_SET_PWR[gSubMenuSelection], gSubMenu_SET_PWR_CN[gSubMenuSelection]));
			break;
		case MENU_SET_PTT:
			strcpy(out, SUBV(gSubMenu_SET_PTT[gSubMenuSelection], gSubMenu_SET_PTT_CN[gSubMenuSelection]));
			break;
		case MENU_SET_TOT:
		case MENU_SET_EOT:
			strcpy(out, SUBV(gSubMenu_SET_TOT[gSubMenuSelection], gSubMenu_SET_TOT_CN[gSubMenuSelection]));
			break;
		case MENU_SET_LCK:
			strcpy(out, SUBV(gSubMenu_SET_LCK[gSubMenuSelection], gSubMenu_SET_LCK_CN[gSubMenuSelection]));
			break;
		case MENU_SET_MET:
		case MENU_SET_GUI:
			strcpy(out, SUBV(gSubMenu_SET_MET[gSubMenuSelection], gSubMenu_SET_MET_CN[gSubMenuSelection]));
			break;
		case MENU_TX_LOCK:
			if (TX_freq_check(gEeprom.VfoInfo[gEeprom.TX_VFO].pTX->Frequency) == 0)
				strcpy(out, SUBV("Inside\nF Lock\nPlan", gSubMenu_TX_LOCK_INSIDE_CN));
			else
				strcpy(out, SUBV(gSubMenu_OFF_ON[gSubMenuSelection], gSubMenu_OFF_ON_CN[gSubMenuSelection]));
			break;
		case MENU_VOL:
			/* Liste : apercu fixe "Syrup" ; le popup garde les pages SysInf. */
			strcpy(out, AUTHOR_STRING_2);
			break;
#ifdef ENABLE_FEAT_F4HWN_SLEEP
		case MENU_SET_OFF:
			if (gSubMenuSelection == 0)
				strcpy(out, SUBV(gSubMenu_OFF_ON[0], gSubMenu_OFF_ON_CN[0]));
			else if (gSubMenuSelection < 121)
				sprintf(out, "%dh:%02dm", (int)(gSubMenuSelection / 60), (int)(gSubMenuSelection % 60));
			break;
#endif
#ifdef ENABLE_FEAT_F4HWN_SCAN_FASTER
		case MENU_SET_SCN:
			strcpy(out, SUBV(gSubMenu_SET_SCN[gSubMenuSelection], gSubMenu_SET_SCN_CN[gSubMenuSelection]));
			break;
#endif
#ifdef ENABLE_FEAT_F4HWN_NARROWER
		case MENU_SET_NFM:
			strcpy(out, SUBV(gSubMenu_SET_NFM[gSubMenuSelection], gSubMenu_SET_NFM_CN[gSubMenuSelection]));
			break;
#endif
#ifdef ENABLE_FEAT_F4HWN_RESCUE_OPS
		case MENU_SET_KEY:
			strcpy(out, SUBV(gSubMenu_SET_KEY[gSubMenuSelection], gSubMenu_SET_KEY_CN[gSubMenuSelection]));
			break;
#endif
#ifdef ENABLE_FEAT_F4HWN_VOL
		case MENU_SET_VOL:
			if (gSubMenuSelection == 0)
				strcpy(out, SUBV(gSubMenu_OFF_ON[0], gSubMenu_OFF_ON_CN[0]));
			else if (gSubMenuSelection < 64)
				sprintf(out, "%02u", (unsigned)gSubMenuSelection);
			break;
#endif
#endif
		default:
			sprintf(out, "%d", (int)gSubMenuSelection);
			break;
	}

	if (strlen(out) >= out_len)
		out[out_len - 1u] = '\0';
}

static void UI_MENU_GetItemPreview(uint8_t view_pos, char *out, uint8_t out_len)
{
	const uint8_t saved_cursor = gMenuCursor;
	const int32_t saved_sel    = gSubMenuSelection;
	char          tmp[64];
	int           m;
#ifdef ENABLE_CHINESE
	const uint8_t saved_lang = gUiLanguage;
#endif

	out[0] = '\0';
	if (view_pos >= gMenuListCount || out_len == 0)
		return;

	gMenuCursor = view_pos;
	MENU_ShowCurrentSetting();
	m = UI_MENU_GetCurrentMenuId();
#ifdef ENABLE_CHINESE
	/* List right column: always English 3x5; Chinese only in submenu popup */
	gUiLanguage = UI_LANGUAGE_EN;
#endif
	UI_MENU_FormatValue(m, tmp, sizeof(tmp));
	UI_MENU_FirstLine(out, tmp, out_len);
#ifdef ENABLE_CHINESE
	gUiLanguage = saved_lang;
#endif

	gMenuCursor       = saved_cursor;
	gSubMenuSelection = saved_sel;
}

/* Popup / panneau : small si CUSTOM (toujours en sous-menu ici), sinon grande police */
static void UI_MENU_PrintValue(const char *str, unsigned x1, unsigned x2, uint8_t line)
{
#ifdef ENABLE_CUSTOM_MENU_LAYOUT
	UI_PrintStringSmallNormal(str, x1, x2, line);
#else
	UI_PrintString(str, x1, x2, line, 8);
#endif
}

#ifndef ENABLE_CHINESE
static void UI_MENU_DrawEditCaret(uint8_t x0, uint8_t pitch, uint8_t bar_w, int16_t uy, int16_t cy)
{
	uint8_t x = x0;
	for (uint8_t ci = 0; ci < 10; ci++)
	{
		if (ci != edit_index) {
			if (edit[ci] != 'g' && edit[ci] != 'j')
				UI_DrawLineBuffer(gFrameBuffer, x, uy, x + bar_w, uy, 1);
		} else {
			UI_DrawLineBuffer(gFrameBuffer, x + 1, cy, x + bar_w - 1, cy, 1);
			UI_DrawPixelBuffer(gFrameBuffer, x + 2, uy, 1);
		}
		x = (uint8_t)(x + pitch);
	}
}
#endif

static uint8_t UI_MENU_SmallTextWidth(const char *text)
{
	return (uint8_t)(strlen(text) * (ARRAY_SIZE(gFontSmall[0]) + 1u));
}

static uint8_t UI_MENU_SmallestTextWidth(const char *text)
{
	return (uint8_t)(strlen(text) * 4u); /* gFont3x5 : 3px + 1px gap */
}

static void UI_MENU_DrawSmallCenteredAtY(const char *text, uint8_t y_top)
{
	uint8_t tw;
	uint8_t tx;

	if (text == NULL || text[0] == '\0')
		return;

#ifdef ENABLE_CHINESE
	/* UTF-8 channel names / CN labels: 12px band */
	if (UI_SmallLinePixelHeight(text) > 8u) {
		UI_PrintStringSmallChannelNameBand(text, 0, LCD_WIDTH - 1, y_top);
		return;
	}
#endif

	tw = UI_MENU_SmallTextWidth(text);
	tx = (LCD_WIDTH > tw) ? (uint8_t)((LCD_WIDTH - tw) / 2u) : 0u;
	UI_MENU_DrawSmallAtY(text, tx, y_top);
}

/* ASCII small = 8px; CJK channel names / CN labels use 12px band. */
static uint8_t UI_MENU_StackedLineHeight(const char *text)
{
#ifdef ENABLE_CHINESE
	return UI_SmallLinePixelHeight(text);
#else
	(void)text;
	return 8u;
#endif
}

#ifdef ENABLE_CHINESE
/* After '\n' was replaced with '\0': walk printable runs into line pointers. */
static uint8_t UI_MENU_SplitNulLines(const char *buf, unsigned len, const char **out, uint8_t max_lines)
{
	unsigned i = 0;
	uint8_t  n = 0;

	while (i < len && n < max_lines) {
		out[n++] = buf + i;
		while (i < len && (uint8_t)buf[i] >= 32u)
			i++;
		while (i < len && (uint8_t)buf[i] < 32u)
			i++;
	}
	return n;
}
#endif

/* Empile des lignes small centrees : ecart fixe gap_px, bloc centre dans [area_y0, area_y1).
 * Line pitch follows each line's real height so a 12px CJK name does not overlap the next row. */
#ifndef ENABLE_CHINESE
static uint8_t UI_MENU_StackedLineY(uint8_t index, uint8_t count, uint8_t area_y0, uint8_t area_y1, uint8_t gap_px)
{
	const uint8_t line_h = 8u;
	uint8_t       block_h;
	uint8_t       area_h;
	uint8_t       y0;

	if (count == 0 || area_y1 <= area_y0)
		return area_y0;

	block_h = (uint8_t)(count * line_h + (uint8_t)((count - 1u) * gap_px));
	area_h  = (uint8_t)(area_y1 - area_y0);
	y0 = (block_h <= area_h) ? (uint8_t)(area_y0 + (area_h - block_h) / 2u) : area_y0;
	return (uint8_t)(y0 + (uint8_t)(index * (line_h + gap_px)));
}
#endif

static void UI_MENU_DrawStackedSmall(
	const char *const *texts,
	uint8_t count,
	uint8_t area_y0,
	uint8_t area_y1,
	uint8_t gap_px)
{
	uint8_t heights[3];
	uint8_t i;
	uint8_t block_h;
	uint8_t area_h;
	uint8_t y;

	if (count == 0 || area_y1 <= area_y0)
		return;
	if (count > 3u)
		count = 3u;

	block_h = 0u;
	for (i = 0; i < count; i++) {
		heights[i] = UI_MENU_StackedLineHeight(texts[i]);
		block_h = (uint8_t)(block_h + heights[i]);
		if (i > 0u)
			block_h = (uint8_t)(block_h + gap_px);
	}

	area_h = (uint8_t)(area_y1 - area_y0);
	y = (block_h <= area_h) ? (uint8_t)(area_y0 + (area_h - block_h) / 2u) : area_y0;

	for (i = 0; i < count; i++) {
		UI_MENU_DrawSmallCenteredAtY(texts[i], y);
		y = (uint8_t)(y + heights[i] + gap_px);
	}
}

/* 2 / 3 lignes, ecart 2px, centre vertical (SysInf, ChName, ChSave, ...). */
static void UI_MENU_DrawLines3(const char *a, const char *b, const char *c, uint8_t area_y0, uint8_t area_y1)
{
	const char *lines[3];
	lines[0] = a;
	lines[1] = b;
	lines[2] = c;
	UI_MENU_DrawStackedSmall(lines, 3, area_y0, area_y1, 2u);
}

static void UI_MENU_DrawLines2(const char *a, const char *b, uint8_t area_y0, uint8_t area_y1)
{
	const char *lines[2];
	lines[0] = a;
	lines[1] = b;
	UI_MENU_DrawStackedSmall(lines, 2, area_y0, area_y1, 2u);
}

// Liste : index (3x5 centre vertical), nom, valeur a droite.
static void UI_MENU_DrawNumberedMenuList(const int count, const int cursor)
{
	char          num[4];
	char          val[24];
#ifdef ENABLE_CHINESE
	/* CN titles are 12px; use taller rows so glyphs do not overlap.
	 * Title "菜单" spans y0..11 — shift divider + list down 5px so they are not clipped. */
	const bool    cn_list   = (gUiLanguage == UI_LANGUAGE_CN);
	const uint8_t cn_shift  = cn_list ? 5u : 0u;
	const uint8_t list_y0   = (uint8_t)((16u - 3u) + cn_shift);
	const uint8_t row_pitch = cn_list ? 13u : (uint8_t)(8u + 1u);
	const uint8_t row_h     = cn_list ? 12u : 8u;
	const int     visible   = cn_list ? 3 : 5;
	const uint8_t div_y     = (uint8_t)(8u + 2u + cn_shift);
#else
	const uint8_t list_y0   = 16u - 3u;
	const uint8_t row_pitch = 8u + 1u;
	const uint8_t row_h     = 8u;
	const int     visible   = 5;
	const uint8_t div_y     = 8u + 2u;
#endif
	int           top;
	int           i;

#ifdef ENABLE_CHINESE
	if (cn_list)
		/* "菜单" — left aligned, ↓2px so it sits clearer under status */
		UI_PrintStringSmallAtPixel("\xe8\x8f\x9c\xe5\x8d\x95", 0, 0, 2u, 13u, 0u);
	else
#endif
		UI_PrintStringSmallBold("MENU", 0, 0, 0);

#ifdef ENABLE_CHINESE
	if (!cn_list)
#endif
	{
		/* EN only: clear fb line 1 under ASCII "MENU". CN must keep line 1 (title y8..11). */
		for (uint8_t x = 0; x < LCD_WIDTH; x++)
			gFrameBuffer[1][x] = 0;
	}
	UI_DrawLineBuffer(gFrameBuffer, 0, div_y, LCD_WIDTH - 1, div_y, 1);

	if (count <= 0)
		return;

	top = cursor - (visible / 2);
	if (top > count - visible)
		top = count - visible;
	if (top < 0)
		top = 0;

	for (i = 0; i < visible; i++)
	{
		const int     idx = top + i;
		const uint8_t y   = (uint8_t)(list_y0 + (uint8_t)(i * row_pitch));
		uint8_t       vw;
		uint8_t       vx;
		uint8_t       name_max_w;
		uint8_t       name_x;

		if (idx >= count)
			break;

		sprintf(num, "%u", (unsigned)(idx + 1));
		/* 3x5 @ x=1, pitch 4px; title 1px after last digit column */
		GUI_DisplaySmallest(num, 1, (uint8_t)(y + 1u), false, true);
		name_x = (uint8_t)(1u + (uint8_t)strlen(num) * 4u + 1u);

		{
			const t_menu_item *item = &MenuList[gMenuIndices[idx]];
			const char *title = UI_MENU_GetMenuTitle(item);
#ifdef ENABLE_CHINESE
			if (cn_list) {
				/* x_end==x_start → left align (AtPixel centers when end>start) */
				UI_PrintStringSmallAtPixel(title, name_x, name_x, y, (uint8_t)(y + 11u), 0u);
				name_max_w = (uint8_t)(name_x + UI_SmallStringPixelWidth(title) + 2u);
			} else
#endif
			{
				UI_MENU_DrawSmallAtY(title, name_x, y);
				name_max_w = (uint8_t)(name_x + UI_MENU_SmallTextWidth(title) + 2u);
			}
		}

		UI_MENU_GetItemPreview((uint8_t)idx, val, sizeof(val));
		/* Right column: English 3x5 only (preview already forced EN) */
		vw = UI_MENU_SmallestTextWidth(val);
		if (vw >= LCD_WIDTH)
			vx = 0;
		else
			vx = (uint8_t)(LCD_WIDTH - vw);

		if (vx < name_max_w)
		{
			while (val[0] != '\0' && (uint8_t)(LCD_WIDTH - UI_MENU_SmallestTextWidth(val)) < name_max_w)
				val[strlen(val) - 1u] = '\0';
			vw = UI_MENU_SmallestTextWidth(val);
			vx = (vw >= LCD_WIDTH) ? 0 : (uint8_t)(LCD_WIDTH - vw);
		}
		if (val[0] != '\0')
			GUI_DisplaySmallest(val, vx, (uint8_t)(y + 1u), false, true);

		if (idx == cursor)
		{
			UI_MENU_XorBand(y, row_h);
			if (y > 0)
				UI_DrawLineBuffer(gFrameBuffer, 0, (int16_t)(y - 1), LCD_WIDTH - 1, (int16_t)(y - 1), 1);
		}
	}
}

#ifdef ENABLE_FEAT_F4HWN_MENU_CAT
// Ecran d'entree menu : liste plate ALL (1.Step, 2.Power, ...).
static void UI_MENU_DrawCategories(void)
{
    UI_DisplayClear();
    UI_MENU_DrawNumberedMenuList(gMenuListCount, gMenuCursor);
    ST7565_BlitFullScreen();
}
#endif

// Construit la "vue" courante = table position affichee -> index MenuList.
// Unique endroit qui fixe gMenuListCount + gMenuIndices.
// CAT_ALL (defaut) = liste plate, identite -> ordre/numeros d'origine preserves.
void UI_MENU_BuildView(void)
{
    gMenuListCount = 0;

#ifdef ENABLE_FEAT_F4HWN_MENU_CAT
    if (gMenuCategory != CAT_ALL)
    {
        const cat_list_t *cl = &CategoryLists[gMenuCategory];
        for (uint8_t k = 0; k < cl->len; k++)
        {
            uint8_t idx;
            if (UI_MENU_ItemHidden(cl->ids[k]))
                continue;
            idx = menu_find_idx(cl->ids[k]);
            if (idx != 0xFF)
                gMenuIndices[gMenuListCount++] = idx;
        }
        return;
    }
#endif

    for (uint8_t i = 0; MenuList[i].name[0] != '\0'; i++)
    {
        if (!gF_LOCK && MenuList[i].menu_id == FIRST_HIDDEN_MENU_ITEM)
            break;

        if (UI_MENU_ItemHidden(MenuList[i].menu_id))
            continue;

        gMenuIndices[gMenuListCount++] = i;
    }
}

int32_t gSubMenuSelection;

// edit box
char    edit_original[17]; // a copy of the text before editing so that we can easily test for changes/difference
char    edit[17];
int     edit_index;
#ifndef ENABLE_CHINESE
bool    edit_is_uppercase = false;
#endif

#ifdef ENABLE_CHINESE
/* Symbol mode: fixed 6-slot strip with numbers 1–6 + symbols */
static void UI_MENU_DrawMemNameSymbolSixPack(unsigned int x1, unsigned int x2, uint8_t y_top, uint8_t y_bot)
{
    static const uint8_t slot_total = 6u;
    const uint8_t n = gMemNameCandidateCount;
    uint8_t slot_index;

    if (n == 0u || x2 <= x1)
        return;

    {
        const unsigned int avail = (unsigned int)((x2 - x1) + 1u);
        const unsigned int w_num = 6u;
        const unsigned int w_sym = 6u;
        const unsigned int gap_num_sym = 4u;

        for (slot_index = 0; slot_index < slot_total; slot_index++)
        {
            char num_str[2];
            char sym_str[2];
            unsigned int slot_l;
            unsigned int slot_r;
            unsigned int slot_w;
            unsigned int token_w;
            unsigned int token_x;
            unsigned int sym_x;

            slot_l = x1 + (avail * (unsigned int)slot_index) / (unsigned int)slot_total;
            slot_r = x1 + (avail * ((unsigned int)slot_index + 1u)) / (unsigned int)slot_total - 1u;

            if (slot_r >= slot_l)
                slot_w = slot_r - slot_l + 1u;
            else
                slot_w = 0u;

            token_w = w_num + gap_num_sym + w_sym;
            token_x = slot_l;
            if (slot_w > token_w)
                token_x = slot_l + (slot_w - token_w) / 2u;

            num_str[0] = (char)('1' + slot_index);
            num_str[1] = 0;
            UI_PrintStringSmallAtPixel(num_str, token_x, token_x, y_top, y_bot, 0u);

            sym_x = token_x + w_num + gap_num_sym;

            if (slot_index < n)
            {
                sym_str[0] = gMemNameCandidates[slot_index];
                sym_str[1] = 0;
            }
            else
            {
                sym_str[0] = ' ';
                sym_str[1] = 0;
            }
            UI_PrintStringSmallAtPixel(sym_str, sym_x, sym_x, y_top, y_bot, 0u);
        }
    }
}

static void UI_MENU_DrawMemNameEdit(unsigned int sub_val_x1, unsigned int sub_val_x2, size_t max_b)
{
    /*
     * Layout (title already at y~0..11):
     *   mode        y_mode .. +7
     *   name glyphs y_name .. name_bot (12px CJK band)
     *   underline   ul_y = just below glyphs (not through them)
     *   candidates  y_strip .. strip_bot (same Y for selected/unselected)
     */
    const uint8_t y_mode    = 13u;
    const uint8_t y_name    = 20u;
    const uint8_t name_bot  = (uint8_t)(y_name + 11u);
    const uint8_t ul_y      = (uint8_t)(name_bot + 1u);
    const uint8_t y_strip   = (uint8_t)(ul_y + 4u);
    const uint8_t strip_bot = (uint8_t)(y_strip + 11u);

    switch (gMemNameInputMode)
    {
        case MEM_NAME_INPUT_DIGIT:
            UI_PrintStringSmallAtPixel("1", (uint8_t)(sub_val_x2 - 6), (uint8_t)sub_val_x2, y_mode, (uint8_t)(y_mode + 7u), 0u);
            break;
        case MEM_NAME_INPUT_LOWER:
            UI_PrintStringSmallAtPixel("a", (uint8_t)(sub_val_x2 - 6), (uint8_t)sub_val_x2, y_mode, (uint8_t)(y_mode + 7u), 0u);
            break;
        case MEM_NAME_INPUT_UPPER:
            UI_PrintStringSmallAtPixel("A", (uint8_t)(sub_val_x2 - 6), (uint8_t)sub_val_x2, y_mode, (uint8_t)(y_mode + 7u), 0u);
            break;
        case MEM_NAME_INPUT_SYMBOL:
            UI_PrintStringSmallAtPixel(",", (uint8_t)(sub_val_x2 - 6), (uint8_t)sub_val_x2, y_mode, (uint8_t)(y_mode + 7u), 0u);
            break;
        default:
            UI_PrintStringSmallAtPixel("PY", (uint8_t)(sub_val_x2 - 12), (uint8_t)sub_val_x2, y_mode, (uint8_t)(y_mode + 7u), 0u);
            break;
    }

    {
        const uint8_t eng_cw = 6;
        const uint8_t chn_cw = 12;
        const uint8_t ul_spacing = 1;
        uint8_t x = (uint8_t)sub_val_x1;
        size_t bi = 0;
        uint8_t slot_x[16];
        uint8_t slot_w[16];
        uint8_t slot_count = 0;
        int8_t cursor_slot = -1;

        while (bi < max_b && x < sub_val_x2 && slot_count < 15)
        {
            slot_x[slot_count] = x;
            if ((uint8_t)edit[bi] >= 0xE4 && (uint8_t)edit[bi] <= 0xEF)
            {
                char ch[4] = { edit[bi], edit[bi + 1], edit[bi + 2], 0 };
                UI_PrintStringSmallAtPixel(ch, x, x, y_name, name_bot, 0u);
                slot_w[slot_count] = chn_cw;
                if (edit_index >= 0 && (size_t)edit_index == bi)
                    cursor_slot = (int8_t)slot_count;
                x += chn_cw + ul_spacing;
                bi += 3;
            }
            else if (edit[bi] == MEM_NAME_EDIT_PAD || edit[bi] == 0)
            {
                slot_w[slot_count] = eng_cw;
                if (edit_index >= 0 && (size_t)edit_index == bi)
                    cursor_slot = (int8_t)slot_count;
                x += eng_cw + ul_spacing;
                bi++;
            }
            else
            {
                char ch[2] = { edit[bi], 0 };
                UI_PrintStringSmallAtPixel(ch, x, x, y_name, name_bot, 0u);
                slot_w[slot_count] = eng_cw;
                if (edit_index >= 0 && (size_t)edit_index == bi)
                    cursor_slot = (int8_t)slot_count;
                x += eng_cw + ul_spacing;
                bi++;
            }
            slot_count++;
        }

        while (bi < max_b && slot_count < 15 && x + eng_cw <= sub_val_x2)
        {
            slot_x[slot_count] = x;
            slot_w[slot_count] = eng_cw;
            if (edit_index >= 0 && (size_t)edit_index == bi)
                cursor_slot = (int8_t)slot_count;
            bi++;
            x += eng_cw + ul_spacing;
            slot_count++;
        }

        if (edit_index >= 0 && (size_t)edit_index == max_b && slot_count < 15 && x + eng_cw <= sub_val_x2)
        {
            slot_x[slot_count] = x;
            slot_w[slot_count] = eng_cw;
            cursor_slot = (int8_t)slot_count;
            slot_count++;
        }

        if (slot_count > 0 && ul_y < (uint8_t)(FRAME_LINES * 8u))
        {
            const uint8_t ul_fb_row = (uint8_t)(ul_y / 8u);
            const uint8_t ul_fb_bit = (uint8_t)(1u << (ul_y % 8u));
            uint8_t s;

            if (ul_fb_row < FRAME_LINES)
            {
                for (s = 0; s < slot_count; s++)
                {
                    uint8_t c;
                    if (cursor_slot >= 0 && (int8_t)s == cursor_slot)
                    {
                        const uint8_t ul2 = (uint8_t)(ul_y + 1u);
                        const uint8_t r2 = (uint8_t)(ul2 / 8u);
                        const uint8_t b2 = (uint8_t)(1u << (ul2 % 8u));
                        for (c = 0; c < slot_w[s]; c++)
                        {
                            gFrameBuffer[ul_fb_row][slot_x[s] + c] |= ul_fb_bit;
                            if (r2 < FRAME_LINES)
                                gFrameBuffer[r2][slot_x[s] + c] |= b2;
                        }
                    }
                    else
                    {
                        for (c = 0; c < slot_w[s]; c++)
                            gFrameBuffer[ul_fb_row][slot_x[s] + c] |= ul_fb_bit;
                    }
                }
            }
        }
    }

    if (gMemNameInputMode == MEM_NAME_INPUT_PINYIN && gPinyinDigitLen > 0)
    {
        char digits[7];
        uint8_t i;
        for (i = 0; i < gPinyinDigitLen && i < 6; i++)
            digits[i] = gPinyinDigitSeq[i];
        digits[i] = 0;
        UI_PrintStringSmallAtPixel(digits, (uint8_t)(sub_val_x2 - 33), (uint8_t)(sub_val_x2 - 3), y_name, name_bot, 0u);
    }

    if (gAskForConfirmation == 0)
    {
        if (gMemNameInputMode == MEM_NAME_INPUT_PINYIN && gPinyinCandidateCount > 0)
        {
            uint8_t x = (uint8_t)sub_val_x1;
            uint8_t i;

            MENU_EnsurePinyinPageVisible();
            for (i = gPinyinCandidateOffset; i < gPinyinCandidateCount; i++)
            {
                uint8_t py_w = (uint8_t)strlen(gPinyinCandidates[i]) * 6u;
                if (x + py_w > sub_val_x2) break;

                if (i == gPinyinCandidateIndex) {
                    uint8_t inv_x1 = (x >= 2u) ? (uint8_t)(x - 2u) : 0u;
                    uint8_t inv_x2 = (uint8_t)(x + py_w + 2u);
                    UI_PrintStringSmallAtPixelCnInverse(gPinyinCandidates[i], inv_x1, inv_x2, y_strip, strip_bot);
                } else
                    UI_PrintStringSmallAtPixel(gPinyinCandidates[i], x, x, y_strip, strip_bot, 0u);
                x += py_w + 6u;
            }
        }
        else if (gCNCandidateCount > 0)
        {
            const unsigned strip_w = (unsigned)(sub_val_x2 - sub_val_x1);
            const unsigned slot_w = strip_w / 6u;
            uint8_t i;

            for (i = 0; i < gCNCandidateCount; i++)
            {
                char num[2];
                char utf8[4];
                uint16_t unicode = gCNCandidates[i];
                const uint8_t cx = (uint8_t)(sub_val_x1 + (unsigned)i * slot_w);

                num[0] = (char)('1' + i);
                num[1] = 0;
                UI_PrintStringSmallAtPixel(num, cx, cx, y_strip, strip_bot, 0u);

                utf8[0] = (char)(0xE0 | (unicode >> 12));
                utf8[1] = (char)(0x80 | ((unicode >> 6) & 0x3F));
                utf8[2] = (char)(0x80 | (unicode & 0x3F));
                utf8[3] = 0;
                UI_PrintStringSmallAtPixel(utf8, (uint8_t)(cx + 8u), (uint8_t)(cx + 8u), y_strip, strip_bot, 0u);
            }
        }
        else if (gMemNameInputMode == MEM_NAME_INPUT_SYMBOL)
        {
            UI_MENU_DrawMemNameSymbolSixPack(sub_val_x1, sub_val_x2, y_strip, strip_bot);
        }
        else if (gMemNameCandidateCount > 0)
        {
            const unsigned strip_w = (unsigned)(sub_val_x2 - sub_val_x1);
            const unsigned slot_w = strip_w / 4u;
            uint8_t i;

            for (i = 0; i < gMemNameCandidateCount; i++)
            {
                char num[2];
                char ch[2];
                const uint8_t cx = (uint8_t)(sub_val_x1 + (unsigned)i * slot_w);

                num[0] = (char)('1' + i);
                num[1] = 0;
                UI_PrintStringSmallAtPixel(num, cx, cx, y_strip, strip_bot, 0u);

                ch[0] = gMemNameCandidates[i];
                ch[1] = 0;
                UI_PrintStringSmallAtPixel(ch, (uint8_t)(cx + 8u), (uint8_t)(cx + 8u), y_strip, strip_bot, 0u);
            }
        }
    }
}
#endif /* ENABLE_CHINESE */

static void UI_MENU_DrawTopRightRoundedBadge(const char *text, const uint8_t line, const bool center_in_area, const uint8_t area_x1, const uint8_t area_x2)
{
    const size_t length = strlen(text);
    const size_t char_pitch = ARRAY_SIZE(gFontSmall[0]) + 1u;
    const size_t text_width = length * char_pitch;
    const size_t capsule_span = text_width + 1u; // matches UI_PrintStringSmallNormalInverse x_end computation
    uint8_t text_x;

    if (length == 0 || line == 0 || line >= FRAME_LINES) {
        return;
    }

    if (center_in_area && area_x2 > area_x1 + 2u) {
        const uint8_t min_x = area_x1 + 1u;
        uint8_t max_x;
        const uint8_t area_width = area_x2 - area_x1 + 1u;

        if (capsule_span >= area_width) {
            text_x = min_x;
        } else {
            text_x = (uint8_t)(area_x1 + ((area_width - capsule_span) / 2u));
        }

        if (area_x2 > capsule_span) {
            max_x = (uint8_t)(area_x2 - capsule_span);
        } else {
            max_x = min_x;
        }

        if (max_x < min_x) {
            max_x = min_x;
        }
        if (text_x < min_x) {
            text_x = min_x;
        } else if (text_x > max_x) {
            text_x = max_x;
        }
    } else {
        if (capsule_span >= (LCD_WIDTH - 3u)) {
            text_x = 1u;
        } else {
            const uint8_t global_shift_right = 1u;
            const uint8_t base_text_x = (uint8_t)(LCD_WIDTH - capsule_span - 3u);
            const uint8_t max_text_x  = (uint8_t)(LCD_WIDTH - capsule_span - 1u);
            const uint16_t shifted_x = (uint16_t)base_text_x + global_shift_right;

            if (shifted_x > max_text_x) {
                text_x = max_text_x;
            } else {
                text_x = (uint8_t)shifted_x;
            }
        }
    }

    UI_PrintStringSmallNormalInverse(text, text_x, 0, line);
}

void UI_DisplayMenu(void)
{
    const unsigned int menu_list_width = 6; // max no. of characters on the menu list (left side)
    unsigned int       menu_item_x1    = (8 * menu_list_width) + 2;
    unsigned int       menu_item_x2    = LCD_WIDTH - 1;
    unsigned int       i;
    char               String[64];  // bigger cuz we can now do multi-line in one string (use '\n' char)
    char               top_right_badge[16];

#ifdef ENABLE_FEAT_F4HWN_MENU_CAT
    if (gMenuLevel == MENU_LEVEL_CAT)
    {
        UI_MENU_DrawCategories();
        return;
    }
#endif

    const int m = UI_MENU_GetCurrentMenuId();

#ifdef ENABLE_DTMF_CALLING
    char               Contact[16];
#endif

    UI_DisplayClear();

#ifndef ENABLE_CUSTOM_MENU_LAYOUT
        // original menu layout
    for (i = 0; i < 3; i++)
        if (gMenuCursor > 0 || i > 0)
            if ((gMenuListCount - 1) != gMenuCursor || i != 2)
                UI_PrintString(UI_MENU_GetMenuTitle(&MenuList[gMenuIndices[gMenuCursor + i - 1]]), 0, 0, i * 2, 8);

    // invert the current menu list item pixels
    for (i = 0; i < (8 * menu_list_width); i++)
    {
        gFrameBuffer[2][i] ^= 0xFF;
        gFrameBuffer[3][i] ^= 0xFF;
    }

    // draw vertical separating dotted line
    for (i = 0; i < 7; i++)
        gFrameBuffer[i][(8 * menu_list_width) + 1] = 0xAA;

    // draw the little sub-menu triangle marker
    if (gIsInSubMenu)
        memcpy(gFrameBuffer[0] + (8 * menu_list_width) + 1, BITMAP_CurrentIndicator, sizeof(BITMAP_CurrentIndicator));

    // draw the menu index number/count
    sprintf(String, "%2u.%u", 1 + gMenuCursor, gMenuListCount);

    UI_PrintStringSmallNormal(String, 2, 0, 6);

#else
    {   // liste + popup edition
        const int menu_index = gMenuCursor;
        const int menu_count = (int)gMenuListCount;

        if (menu_index >= 0 && menu_index < menu_count)
        {
            if (!gIsInSubMenu)
            {
                UI_MENU_DrawNumberedMenuList(menu_count, menu_index);
                BACKLIGHT_TurnOn();
                ST7565_BlitFullScreen();
                return;
            }

            /* Popup : titre gauche, contenu plein ecran */
            menu_item_x1 = 2;
            menu_item_x2 = LCD_WIDTH - 1;
            {
                const t_menu_item *item = &MenuList[gMenuIndices[menu_index]];
                const char *title = UI_MENU_GetMenuTitle(item);
#ifdef ENABLE_CHINESE
                if (gUiLanguage == UI_LANGUAGE_CN)
                    /* x_end==x_start → left align */
                    UI_PrintStringSmallAtPixel(title, 0, 0, 0, 11u, 0u);
                else
#endif
                    UI_PrintString(title, 0, 0, 0, 8);
            }
        }
    }
#endif

    // **************

    String[0] = '\0';
    top_right_badge[0] = '\0';

    bool already_printed = false;

    /* Brightness is set to max in some entries of this menu. Return it to the configured brightness
       level the "next" time we enter here.I.e., when we move from one menu to another.
       It also has to be set back to max when pressing the Exit key. */

    BACKLIGHT_TurnOn();

    //#if !defined(ENABLE_SPECTRUM) || !defined(ENABLE_FMRADIO)
        uint8_t gaugeLine = 0;
        uint8_t gaugeMin = 0;
        uint8_t gaugeMax = 0;
    //#endif

    /* Popup : contenu sous le titre, police small (1 ligne fb chacune) */
    const uint8_t popup_line0 = gIsInSubMenu ? 2 : 0;
    const uint8_t popup_line4 = gIsInSubMenu ? 4 : 4;
    const uint8_t popup_line5 = gIsInSubMenu ? 5 : 5;

    switch (m)
    {
        case MENU_SQL:
        case MENU_MIC:
        case MENU_MIC_BAR:
        case MENU_STEP:
        case MENU_TXP:
        case MENU_R_DCS:
        case MENU_T_DCS:
        case MENU_R_CTCS:
        case MENU_T_CTCS:
        case MENU_SFT_D:
        case MENU_W_N:
        case MENU_AM:
        case MENU_BCL:
        case MENU_BEEP:
        case MENU_STE:
        case MENU_D_ST:
#ifdef ENABLE_DTMF_CALLING
        case MENU_D_DCD:
        case MENU_ANI_ID:
        case MENU_D_RSP:
        case MENU_D_HOLD:
#endif
        case MENU_D_LIVE_DEC:
#ifdef ENABLE_NOAA
        case MENU_NOAA_S:
#endif
        case MENU_350EN:
#ifdef ENABLE_VOICE
        case MENU_VOICE:
#endif
#ifdef ENABLE_ALARM
        case MENU_AL_MOD:
#endif
        case MENU_MDF:
        case MENU_LIST_CH:
        case MENU_S_LIST:
#ifdef ENABLE_FEAT_F4HWN
        case MENU_SET_TMR:
        case MENU_S_PRI:
        case MENU_TX_LOCK:
        case MENU_SET_PTT:
        case MENU_SET_TOT:
        case MENU_SET_EOT:
        case MENU_SET_LCK:
        case MENU_SET_PWR:
        case MENU_SET_MET:
        case MENU_SET_GUI:
#ifdef ENABLE_FEAT_F4HWN_SCAN_FASTER
        case MENU_SET_SCN:
#endif
#ifdef ENABLE_FEAT_F4HWN_NARROWER
        case MENU_SET_NFM:
#endif
#ifdef ENABLE_FEAT_F4HWN_RESCUE_OPS
        case MENU_SET_KEY:
#endif
#if defined(ENABLE_FEAT_F4HWN_LOGO_SAV)
        case MENU_SET_SAV:
#endif
#endif
        case MENU_YAN_ID:
            if (gIsInSubMenu && edit_index >= 0)
            {
#ifdef ENABLE_CHINESE
                UI_MENU_DrawMemNameEdit(2u, (unsigned int)(LCD_WIDTH - 1u), (size_t)YAN_ID_LEN);
#else
                UI_PrintStringSmallNormal(edit, 0, LCD_WIDTH - 1, 3);
#endif
                already_printed = true;
                break;
            }
            UI_MENU_FormatValue(m, String, sizeof(String));
            break;
        case MENU_SAVE:
        case MENU_TDR:
        case MENU_TOT:
        case MENU_ABR:
        case MENU_ABR_MIN:
        case MENU_ABR_MAX:
        case MENU_BAT_TXT:
        case MENU_PONMSG:
        case MENU_ROGER:
        case MENU_ROGER_PREVIEW:
        case MENU_YAN_ID_RX:
        case MENU_PTT_ID:
        case MENU_F_LOCK:
        case MENU_RESET:
        case MENU_BATTYP:
        case MENU_SET_NAV:
        case MENU_F1SHRT:
        case MENU_F1LONG:
        case MENU_F2SHRT:
        case MENU_F2LONG:
        case MENU_MLONG:
        case MENU_VOX:
        case MENU_COMPAND:
        case MENU_ABR_ON_TX_RX:
        case MENU_SC_REV:
        case MENU_AUTOLK:
        case MENU_UPCODE:
        case MENU_DWCODE:
        case MENU_D_PRE:
        case MENU_RP_STE:
        case MENU_BATCAL:
#ifndef ENABLE_FEAT_F4HWN
        case MENU_350TX:
        case MENU_200TX:
        case MENU_500TX:
        case MENU_SCREN:
#ifdef ENABLE_AM_FIX
        case MENU_AM_FIX:
#endif
#endif
        case MENU_LANGUAGE:
            UI_MENU_FormatValue(m, String, sizeof(String));
            if (m == MENU_MIC) {
                gaugeLine = 4; gaugeMin = 0; gaugeMax = 8;
            } else if (m == MENU_TOT) {
                gaugeLine = 4; gaugeMin = 5; gaugeMax = 179;
            } else if (m == MENU_ABR && gSubMenuSelection > 0 && gSubMenuSelection < 61) {
                gaugeLine = 4; gaugeMin = 1; gaugeMax = 60;
            } else if (m == MENU_SC_REV && gSubMenuSelection > 0 && gSubMenuSelection < 81) {
                gaugeLine = 5; gaugeMin = 1; gaugeMax = 80;
            } else if (m == MENU_SC_REV && gSubMenuSelection >= 81) {
                gaugeLine = 5; gaugeMin = 80; gaugeMax = 104;
            } else if (m == MENU_AUTOLK && gSubMenuSelection > 0) {
                gaugeLine = 4; gaugeMin = 1; gaugeMax = 40;
            } else if ((m == MENU_ABR_MIN || m == MENU_ABR_MAX) && gIsInSubMenu) {
                BACKLIGHT_SetBrightness(gSubMenuSelection);
            }
            break;

        case MENU_OFFSET:
            if (!gIsInSubMenu || gInputBoxIndex == 0)
            {
                sprintf(String, "%3d.%05u", gSubMenuSelection / 100000, abs(gSubMenuSelection) % 100000);
            }
            else
            {
                const char * ascii = INPUTBOX_GetAscii();
                sprintf(String, "%.3s.%.3s  ",ascii, ascii + 3);
            }

            UI_MENU_PrintValue(String, menu_item_x1, menu_item_x2, popup_line0 + 1);
            UI_MENU_PrintValue("MHz",  menu_item_x1, menu_item_x2, popup_line0 + 2);

            already_printed = true;
            break;

#ifndef ENABLE_FEAT_F4HWN
        case MENU_SCR:
            UI_MENU_FormatValue(m, String, sizeof(String));
            if (gSubMenuSelection > 0 && gSetting_ScrambleEnable)
                BK4819_EnableScramble(gSubMenuSelection - 1);
            else
                BK4819_DisableScramble();
            break;
#endif

        case MENU_MEM_CH:
        case MENU_1_CALL:
        case MENU_DEL_CH:
        case MENU_S_PRI_CH_1:
        case MENU_S_PRI_CH_2:
        {
            const uint8_t area_y0 = 16u;
            const uint8_t area_y1 = 56u;

            if (gSubMenuSelection == MR_CHANNELS_MAX)
            {
                const char *none = SUBV("None", gSubMenu_MEM_NONE_CN);
                UI_MENU_DrawStackedSmall(&none, 1, area_y0, area_y1, 2u);
            }
            else
            {
                const bool valid = RADIO_CheckValidChannel(gSubMenuSelection, false, 0);
                char       ch[16];
                char       name[17];
                char       freq[16];

                UI_GenerateChannelStringEx(ch, valid, gSubMenuSelection);
                SETTINGS_FetchChannelName(name, gSubMenuSelection);
                if (name[0] == '\0')
                    strcpy(name, "--");

                if (gAskForConfirmation)
                {
                    UI_MENU_DrawLines3(ch, name, SUBV((gAskForConfirmation == 1) ? "SURE?" : "WAIT!",
                        (gAskForConfirmation == 1) ? "\xe7\xa1\xae\xe8\xae\xa4?" : "\xe8\xaf\xb7\xe7\xad\x89\xe5\xbe\x85!"),
                        area_y0, area_y1);
                }
                else if (valid)
                {
                    const uint32_t frequency = SETTINGS_FetchChannelFrequency(gSubMenuSelection);
                    sprintf(freq, "%u.%05u", frequency / 100000, frequency % 100000);
                    UI_MENU_DrawLines3(ch, name, freq, area_y0, area_y1);
                }
                else
                {
                    UI_MENU_DrawLines2(ch, name, area_y0, area_y1);
                }
            }
            already_printed = true;
            break;
        }

        case MENU_MEM_NAME:
        {
            const uint8_t area_y0 = 16u;
            const uint8_t area_y1 = 56u;
            const bool    valid   = RADIO_CheckValidChannel(gSubMenuSelection, false, 0);
            char          ch[16];
            char          name[17];
            char          freq[16];

            UI_GenerateChannelStringEx(ch, valid, gSubMenuSelection);

            if (!valid)
            {
                const char *lines[1];
                lines[0] = ch;
                UI_MENU_DrawStackedSmall(lines, 1, area_y0, area_y1, 2u);
            }
            else
            {
                const uint32_t frequency = SETTINGS_FetchChannelFrequency(gSubMenuSelection);

                if (!gIsInSubMenu)
                    edit_index = -1;

                if (gAskForConfirmation)
                {
                    SETTINGS_FetchChannelName(name, gSubMenuSelection);
                    if (name[0] == '\0')
                        strcpy(name, "--");
                    UI_MENU_DrawLines3(ch, name, SUBV((gAskForConfirmation == 1) ? "SURE?" : "WAIT!",
                        (gAskForConfirmation == 1) ? "\xe7\xa1\xae\xe8\xae\xa4?" : "\xe8\xaf\xb7\xe7\xad\x89\xe5\xbe\x85!"),
                        area_y0, area_y1);
                }
                else if (edit_index < 0)
                {
                    SETTINGS_FetchChannelName(name, gSubMenuSelection);
                    if (name[0] == '\0')
                        strcpy(name, "--");
                    sprintf(freq, "%u.%05u", frequency / 100000, frequency % 100000);
                    UI_MENU_DrawLines3(ch, name, freq, area_y0, area_y1);
                }
                else
                {
#ifdef ENABLE_CHINESE
                    UI_MENU_DrawMemNameEdit(2u, (unsigned int)(LCD_WIDTH - 1u), (size_t)CHANNEL_NAME_MAX_BYTES);
#else
                    const uint8_t y_ch   = UI_MENU_StackedLineY(0, 3, area_y0, area_y1, 2u);
                    const uint8_t y_edit = UI_MENU_StackedLineY(1, 3, area_y0, area_y1, 2u);
                    const uint8_t y_abc  = UI_MENU_StackedLineY(2, 3, area_y0, area_y1, 2u);
#ifdef ENABLE_CUSTOM_MENU_LAYOUT
                    const uint8_t pitch  = (uint8_t)(ARRAY_SIZE(gFontSmall[0]) + 1u);
                    const uint8_t text_w = (uint8_t)(10u * pitch);
                    const uint8_t x0 = (uint8_t)((LCD_WIDTH > text_w) ? ((LCD_WIDTH - text_w) / 2u) : 0u);

                    UI_MENU_DrawSmallCenteredAtY(ch, y_ch);
                    UI_MENU_DrawSmallAtY(edit, x0, y_edit);
                    if (edit_index < 10) {
                        UI_MENU_DrawEditCaret(x0, pitch, 6, (int16_t)(y_edit + 7), (int16_t)(y_edit + 8));
                        UI_MENU_DrawSmallAtY(edit_is_uppercase ? "ABC" : "abc", 77, y_abc);
                    }
#else
                    UI_MENU_DrawSmallCenteredAtY(ch, y_ch);
                    UI_PrintString(edit, menu_item_x1, menu_item_x2, (uint8_t)(y_edit / 8u), 8);
                    if (edit_index < 10) {
                        UI_MENU_DrawEditCaret((uint8_t)(menu_item_x1 - 1), 8, 6, (int16_t)(y_edit + 7), (int16_t)(y_edit + 8));
                        UI_PrintStringSmallNormal(edit_is_uppercase ? "ABC" : "abc", 77, 0, (uint8_t)(y_abc / 8u));
                    }
#endif
#endif /* ENABLE_CHINESE */
                }
            }

            already_printed = true;
            break;
        }

#ifdef ENABLE_DTMF_CALLING
        case MENU_D_LIST:
            gIsDtmfContactValid = DTMF_GetContact((int)gSubMenuSelection - 1, Contact);
            if (!gIsDtmfContactValid)
                strcpy(String, "NULL");
            else
                memcpy(String, Contact, 8);
            break;
#endif

        case MENU_VOL: {
            // SysInf is paginated. Pages appear in this order, only when their
            // feature flag is enabled:
            //   0          -> identity
            //   next       -> Build date/time         (ENABLE_FEAT_F4HWN)
            //   next       -> Battery                 (ENABLE_FEAT_F4HWN)
            //   next       -> Flash / SRAM usage      (ENABLE_FEAT_F4HWN_MEM)
            //   next, +1   -> CODE / WIKI QR codes    (ENABLE_FEAT_F4HWN_QRCODE)
            // In non-F4HWN builds, page 0 keeps the old battery-voltage display.
            const uint8_t page = (uint8_t)gSubMenuSelection;
            uint8_t       p    = 0;
            /* Sous le titre (y16) ; avec badge (ligne 2 = y16..23) : +4px puis contenu */
            const uint8_t area_y0       = 16u;
            const uint8_t area_y1       = 56u;
            const uint8_t area_y0_badge = 24u + 4u;

            if (page == p++) {
#ifdef ENABLE_FEAT_F4HWN
                UI_MENU_DrawLines3(AUTHOR_STRING_2, DISPLAY_VERSION_STRING_2, PACK_SUFFIX, area_y0, area_y1);
#else
                {
                    char v1[16];
                    char v2[16];
                    sprintf(v1, "%u.%02uV",
                        gBatteryVoltageAverage / 100, gBatteryVoltageAverage % 100);
                    sprintf(v2, "%u%%", BATTERY_VoltsToPercent(gBatteryVoltageAverage));
                    UI_MENU_DrawLines2(v1, v2, area_y0, area_y1);
                }
#endif
                already_printed = true;
                break;
            }
#ifdef ENABLE_FEAT_F4HWN
            if (page == p++) {
                strcpy(top_right_badge, "BUILD");
                UI_MENU_DrawLines3(BuildDate, BuildTime, BuildCommit, area_y0_badge, area_y1);
                already_printed = true;
                break;
            }

            if (page == p++) {
                char val[16];

                strcpy(top_right_badge, "BATTERY");
                sprintf(val, "%u.%02uV %u%%",
                    gBatteryVoltageAverage / 100, gBatteryVoltageAverage % 100,
                    BATTERY_VoltsToPercent(gBatteryVoltageAverage));
                UI_MENU_DrawLines2(val, gSubMenu_BATTYP[gEeprom.BATTERY_TYPE], area_y0_badge, area_y1);
                already_printed = true;
                break;
            }
#endif
#ifdef ENABLE_FEAT_F4HWN_MEM
            if (page == p++) {
                uint16_t flash_pct = 0;
                uint16_t ram_pct   = 0;
                char     flash[16];
                char     sram[16];

                UI_GetMemPercents(&flash_pct, &ram_pct);
                strcpy(top_right_badge, "MEMORY");
                sprintf(flash, "FLASH %u.%u%%",
                        (unsigned)(flash_pct / 100), (unsigned)((flash_pct / 10) % 10));
                sprintf(sram, "SRAM  %u.%u%%",
                        (unsigned)(ram_pct / 100), (unsigned)((ram_pct / 10) % 10));
                UI_MENU_DrawLines2(flash, sram, area_y0_badge, area_y1);
                already_printed = true;
                break;
            }
#endif
#ifdef ENABLE_FEAT_F4HWN_QRCODE
            if (page == p || page == p + 1) {
                const bool is_wiki = (page == (p + 1));

                strcpy(top_right_badge, is_wiki ? "WIKI" : "CODE");
                UI_DrawQRCode(is_wiki, 72, 28);

                already_printed = true;
                break;
            }

            p += 2;
#endif
            break;
        }

#ifdef ENABLE_F_CAL_MENU
        case MENU_F_CALI:
            {
                const uint32_t value   = 22656 + gSubMenuSelection;
                const uint32_t xtal_Hz = (0x4f0000u + value) * 5;

                writeXtalFreqCal(gSubMenuSelection, false);

                sprintf(String, "%d\n%u.%06u\nMHz",
                    gSubMenuSelection,
                    xtal_Hz / 1000000, xtal_Hz % 1000000);
            }
            break;
#endif

#ifdef ENABLE_FEAT_F4HWN_SLEEP
        case MENU_SET_OFF:
            UI_MENU_FormatValue(m, String, sizeof(String));
            if (gSubMenuSelection > 0 && gSubMenuSelection < 121) {
                gaugeLine = 4;
                gaugeMin = 1;
                gaugeMax = 120;
            }
            break;
#endif

#ifdef ENABLE_FEAT_F4HWN
        case MENU_SET_CTR:
            UI_MENU_FormatValue(m, String, sizeof(String));
#ifdef ENABLE_FEAT_F4HWN_CTR
            gSetting_set_ctr = gSubMenuSelection;
            ST7565_ContrastAndInv();
#endif
            break;

        case MENU_SET_INV:
            UI_MENU_FormatValue(m, String, sizeof(String));
#ifdef ENABLE_FEAT_F4HWN_INV
            ST7565_ContrastAndInv();
#endif
            break;

#ifdef ENABLE_FEAT_F4HWN_AUDIO
        case MENU_SET_AUD:
            if(gTxVfo->Modulation == MODULATION_AM) {
                strcpy(String, gSubMenu_SET_AUD_AM[gSubMenuSelection]);
                strcpy(top_right_badge, "AM");
            }
            else if (gTxVfo->Modulation == MODULATION_USB) {
                strcpy(String, "USB");
                strcpy(top_right_badge, "USB");
            }
            else {
                strcpy(String, gSubMenu_SET_AUD_FM[gSubMenuSelection]);
                strcpy(top_right_badge, "FM");
            }
            break;
#endif

#ifdef ENABLE_FEAT_F4HWN_VOL
        case MENU_SET_VOL:
            UI_MENU_FormatValue(m, String, sizeof(String));
            if (gSubMenuSelection > 0 && gSubMenuSelection < 64) {
                gaugeLine = 4;
                gaugeMin = 1;
                gaugeMax = 63;
            }
            BK4819_SetRxAudioGain();
            break;
#endif
#endif

    }

    //#if !defined(ENABLE_SPECTRUM) || !defined(ENABLE_FMRADIO)
    /* Gauge apres le texte (popup) : evite de chevaucher, ecart 3px. */
    if (gaugeLine != 0 && !(gIsInSubMenu && !already_printed && String[0] != '\0'))
    {
        ST7565_Gauge(gaugeLine, gaugeMin, gaugeMax, gSubMenuSelection);
    }
    //#endif

    if (!already_printed)
    {   // we now do multi-line text in a single string

        unsigned int y;
        unsigned int lines = 1;
        unsigned int len   = strlen(String);
        bool         small = false;

        if (String[0] != '\0')
        {
            // count number of lines
            for (i = 0; i < len; i++)
            {
                if (String[i] == '\n' && i < (len - 1))
                {   // found new line char
                    lines++;
                    String[i] = 0;  // null terminate the line
                }
            }

            /* Popup + jauge (BLTime, TOT, ...) : texte au-dessus, 3px, puis barre */
            if (gIsInSubMenu && gaugeLine != 0)
            {
                const uint8_t line_h  = 8u;
                const uint8_t gap_px  = 3u;
                const uint8_t title_h = 16u; /* grande police titre */
                int           text_y0;
                unsigned int  li;

                if (lines > 4)
                    lines = 4;

#ifdef ENABLE_CHINESE
                if (gUiLanguage == UI_LANGUAGE_CN) {
                    const char *ml[4];
                    const uint8_t stack_gap = 1u;
                    uint8_t       n;
                    uint8_t       li_h;
                    uint8_t       block_h = 0u;
                    uint8_t       need_y;
                    uint8_t       band_end;

                    n = UI_MENU_SplitNulLines(String, len, ml, (uint8_t)lines);
                    for (li_h = 0; li_h < n; li_h++) {
                        block_h = (uint8_t)(block_h + UI_SmallLinePixelHeight(ml[li_h]));
                        if (li_h > 0u)
                            block_h = (uint8_t)(block_h + stack_gap);
                    }
                    need_y = (uint8_t)(title_h + block_h + gap_px);
                    if (need_y > (uint8_t)(gaugeLine * 8u)) {
                        gaugeLine = (uint8_t)((need_y + 7u) / 8u);
                        if (gaugeLine > 6u)
                            gaugeLine = 6u;
                    }
                    band_end = (uint8_t)(gaugeLine * 8u);
                    if (band_end > gap_px)
                        band_end = (uint8_t)(band_end - gap_px - 1u);
                    if (band_end < title_h)
                        band_end = title_h;
                    UI_PrintStringSmallStackedAtPixel(ml, n,
                        (uint8_t)menu_item_x1, (uint8_t)menu_item_x2,
                        title_h, band_end, stack_gap, 0u);
                } else
#endif
                {
                /* Bas du texte a gauge_y0 - 3px */
                text_y0 = (int)(gaugeLine * 8u) - (int)gap_px - (int)(lines * line_h);
                if (text_y0 < (int)title_h)
                {
                    /* Remonter le texte sous le titre ; jauge a la 1re ligne fb avec ecart >= 3px */
                    text_y0 = (int)title_h;
                    {
                        const uint8_t need_y = (uint8_t)(title_h + lines * line_h + gap_px);
                        gaugeLine = (uint8_t)((need_y + 7u) / 8u);
                        if (gaugeLine > 6u)
                            gaugeLine = 6u;
                    }
                }

                for (li = 0, i = 0; li < lines && i < len; li++)
                {
                    {
                        const uint8_t tw = UI_MENU_SmallTextWidth(String + i);
                        const uint8_t tx = (LCD_WIDTH > tw) ? (uint8_t)((LCD_WIDTH - tw) / 2u) : 0u;

                        UI_MENU_DrawSmallAtY(String + i, tx, (uint8_t)(text_y0 + (int)(li * line_h)));
                    }

                    while (i < len && String[i] >= 32)
                        i++;
                    while (i < len && String[i] < 32)
                        i++;
                }
                }

                ST7565_Gauge(gaugeLine, gaugeMin, gaugeMax, gSubMenuSelection);
            }
            else if (gIsInSubMenu)
            {
                small = true;
                if (lines > 5)
                    lines = 5;

#ifdef ENABLE_CHINESE
                if (gUiLanguage == UI_LANGUAGE_CN || SETTINGS_ChannelNameHasCjkUtf8(String)) {
                    const char *ml[5];
                    const uint8_t n = UI_MENU_SplitNulLines(String, len, ml, (uint8_t)lines);

                    /* Title occupies y0..11 (CN) / y0..15 (EN big). Content band = fb lines 2..6. */
                    UI_PrintStringSmallStackedAtPixel(ml, n,
                        (uint8_t)menu_item_x1, (uint8_t)menu_item_x2,
                        16u, 55u, 1u, 0u);
                } else
#endif
                {
                y = 2u + ((5u - lines) / 2u);

                for (i = 0; i < len && lines > 0; lines--)
                {
                    UI_PrintStringSmallNormal(String + i, menu_item_x1, menu_item_x2, y);

                    while (i < len && String[i] >= 32)
                        i++;
                    while (i < len && String[i] < 32)
                        i++;

                    y += 1;
                }
                }
            }
            else
            {
                if (lines > 3)
                {
                    small = true;
                    if (lines > 7)
                        lines = 7;
                }
                y = (small ? 3 : 2) - (lines / 2);

                for (i = 0; i < len && lines > 0; lines--)
                {
                    if (small)
                        UI_PrintStringSmallNormal(String + i, menu_item_x1, menu_item_x2, y);
                    else
                        UI_PrintString(String + i, menu_item_x1, menu_item_x2, y, 8);

                    while (i < len && String[i] >= 32)
                        i++;
                    while (i < len && String[i] < 32)
                        i++;

                    y += small ? 1 : 2;
                }
            }
        }
    }

    if (m == MENU_S_PRI_CH_1 || m == MENU_S_PRI_CH_2)
    {

    }

    if ((m == MENU_R_CTCS || m == MENU_R_DCS) && gCssBackgroundScan)
    {
        if (gIsInSubMenu)
            UI_PrintStringSmallNormal("SCAN", menu_item_x1, menu_item_x2, popup_line4);
        else
            UI_PrintString("SCAN", menu_item_x1, menu_item_x2, popup_line4, 8);
    }

#ifdef ENABLE_DTMF_CALLING
    if (m == MENU_D_LIST && gIsDtmfContactValid) {
        Contact[11] = 0;
        memcpy(&gDTMF_ID, Contact + 8, 4);
        sprintf(String, "ID:%4s", gDTMF_ID);
        if (gIsInSubMenu)
            UI_PrintStringSmallNormal(String, menu_item_x1, menu_item_x2, popup_line4);
        else
            UI_PrintString(String, menu_item_x1, menu_item_x2, popup_line4, 8);
    }
#endif

    const bool is_ctcs = (m == MENU_R_CTCS || m == MENU_T_CTCS);
    const bool is_dcs  = (m == MENU_R_DCS  || m == MENU_T_DCS);

    if (is_ctcs || is_dcs) {
        if (gSubMenuSelection == 0) {
            strcpy(top_right_badge, is_ctcs ? "00/00" : "000/00");
        } else {
            const uint8_t approved_index = is_ctcs ? 
                DCS_GetCtcssApprovedIndex(gSubMenuSelection - 1) : 
                DCS_GetDcsApprovedIndex(gSubMenuSelection - 1);
                
            const uint8_t width = is_ctcs ? 2 : 3;

            if (approved_index != 0xFF) {
                sprintf(top_right_badge, "%0*u/%02u", width, (unsigned)gSubMenuSelection, (unsigned)approved_index + 1);
            } else {
                sprintf(top_right_badge, "%0*u/--", width, (unsigned)gSubMenuSelection);
            }
        }
    }

#ifdef ENABLE_DTMF_CALLING
    if (m == MENU_D_LIST) {
        sprintf(top_right_badge, "%03d", gSubMenuSelection);
    }
#endif

    if (top_right_badge[0] != '\0') {
        UI_MENU_DrawTopRightRoundedBadge(top_right_badge, gIsInSubMenu ? 2 : 1, true, menu_item_x1, menu_item_x2);
    }

    if ((m == MENU_RESET
#ifndef ENABLE_CUSTOM_MENU_LAYOUT
         || m == MENU_MEM_CH
         || m == MENU_MEM_NAME
         || m == MENU_DEL_CH
#endif
        ) && gAskForConfirmation)
    {   // display confirmation (MEM_* deja inclus dans DrawLines3 en layout custom)
        char *pPrintStr = (char *)SUBV((gAskForConfirmation == 1) ? "SURE?" : "WAIT!",
            (gAskForConfirmation == 1) ? "\xe7\xa1\xae\xe8\xae\xa4?" : "\xe8\xaf\xb7\xe7\xad\x89\xe5\xbe\x85!");
#ifdef ENABLE_CHINESE
        if (gUiLanguage == UI_LANGUAGE_CN && gIsInSubMenu) {
            UI_PrintStringSmallAtPixel(pPrintStr, (uint8_t)menu_item_x1, (uint8_t)menu_item_x2,
                (uint8_t)(popup_line5 * 8u), (uint8_t)(popup_line5 * 8u + 11u), 0u);
        } else
#endif
        if (gIsInSubMenu)
            UI_PrintStringSmallNormal(pPrintStr, menu_item_x1, menu_item_x2, popup_line5);
        else
            UI_PrintString(pPrintStr, menu_item_x1, menu_item_x2, popup_line5, 8);
    }

    ST7565_BlitFullScreen();
}
