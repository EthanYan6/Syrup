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


const t_menu_item MenuList[] =
{
//   text,          menu ID
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
    "MAIN TX\nDUAL RX"  // always TX on main, but RX on both
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
    "ALL",
    "SOUND",
#else
    "FULL",
#endif
    "MESSAGE",
    "VOLTAGE",
#ifdef ENABLE_FEAT_F4HWN_LOGO
    "LOGO",
#endif
    "NONE"
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
    "MDC"
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
static const uint8_t CatDisplay[] = { MENU_PONMSG, MENU_ABR, MENU_ABR_MIN, MENU_ABR_MAX, MENU_ABR_ON_TX_RX, MENU_SET_CTR, MENU_SET_INV, MENU_VOL };
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
static const uint8_t CatRadio[]   = { MENU_SQL, MENU_STE, MENU_RP_STE, MENU_ROGER, MENU_VOX, MENU_TDR };
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
            n++;
        }
        return n;
    }

    const cat_list_t *cl = &CategoryLists[cat];
    for (uint8_t k = 0; k < cl->len; k++)
        if (menu_find_idx(cl->ids[k]) != 0xFF)
            n++;
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
				strcpy(out, gSubMenu_TXP[0]);
			else
#ifdef ENABLE_FEAT_F4HWN
				sprintf(out, "%s\n%sW", gSubMenu_TXP[gSubMenuSelection], gSubMenu_SET_PWR[gSubMenuSelection - 1]);
#else
				strcpy(out, gSubMenu_TXP[gSubMenuSelection]);
#endif
			break;
		case MENU_R_DCS:
		case MENU_T_DCS:
			if (gSubMenuSelection == 0)
				strcpy(out, gSubMenu_OFF_ON[0]);
			else if (gSubMenuSelection < 105)
				sprintf(out, "D%03oN", DCS_Options[gSubMenuSelection - 1]);
			else
				sprintf(out, "D%03oI", DCS_Options[gSubMenuSelection - 105]);
			break;
		case MENU_R_CTCS:
		case MENU_T_CTCS:
			if (gSubMenuSelection == 0)
				strcpy(out, gSubMenu_OFF_ON[0]);
			else
				sprintf(out, "%u.%uHz", CTCSS_Options[gSubMenuSelection - 1] / 10, CTCSS_Options[gSubMenuSelection - 1] % 10);
			break;
		case MENU_SFT_D:
			strcpy(out, gSubMenu_SFT_D[gSubMenuSelection]);
			break;
		case MENU_OFFSET:
			sprintf(out, "%d.%05u", (int)(gSubMenuSelection / 100000), (unsigned)(abs(gSubMenuSelection) % 100000));
			break;
		case MENU_W_N:
			strcpy(out, gSubMenu_W_N[gSubMenuSelection]);
			break;
		case MENU_AM:
			strcpy(out, gModulationStr[gSubMenuSelection]);
			break;
		case MENU_MEM_NAME:
			SETTINGS_FetchChannelName(out, (uint16_t)gSubMenuSelection);
			if (out[0] == '\0')
				strcpy(out, "--");
			break;
		case MENU_MEM_CH:
		case MENU_DEL_CH:
		case MENU_1_CALL:
		case MENU_S_PRI_CH_1:
		case MENU_S_PRI_CH_2:
			if (gSubMenuSelection == MR_CHANNELS_MAX)
				strcpy(out, "None");
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
			strcpy(out, gSubMenu_OFF_ON[gSubMenuSelection]);
			break;
		case MENU_MIC_BAR:
#ifdef ENABLE_AUDIO_BAR
			strcpy(out, gSubMenu_OFF_ON[gSubMenuSelection]);
#else
			strcpy(out, gSubMenu_NA);
#endif
			break;
		case MENU_SAVE:
			if (gSubMenuSelection == 0)
				strcpy(out, gSubMenu_OFF_ON[0]);
			else
				sprintf(out, "1:%u", (unsigned)gSubMenuSelection);
			break;
		case MENU_TDR:
			strcpy(out, gSubMenu_RXMode[gSubMenuSelection]);
			break;
		case MENU_TOT:
			sprintf(out, "%02dm:%02ds", (int)((((gSubMenuSelection + 1) * 5) / 60)), (int)((((gSubMenuSelection + 1) * 5) % 60)));
			break;
		case MENU_ABR:
			if (gSubMenuSelection == 0)
				strcpy(out, gSubMenu_OFF_ON[0]);
			else if (gSubMenuSelection < 61)
				sprintf(out, "%02dm:%02ds", (int)((gSubMenuSelection * 5) / 60), (int)((gSubMenuSelection * 5) % 60));
			else
				strcpy(out, "ON");
			break;
		case MENU_ABR_MIN:
		case MENU_ABR_MAX:
#if defined(ENABLE_FEAT_F4HWN) && defined(ENABLE_FEAT_F4HWN_CTR)
		case MENU_SET_CTR:
#endif
			sprintf(out, "%d", (int)gSubMenuSelection);
			break;
		case MENU_BAT_TXT:
			strcpy(out, gSubMenu_BAT_TXT[gSubMenuSelection]);
			break;
		case MENU_PONMSG:
			strcpy(out, gSubMenu_PONMSG[gSubMenuSelection]);
			break;
		case MENU_ROGER:
			strcpy(out, gSubMenu_ROGER[gSubMenuSelection]);
			break;
		case MENU_PTT_ID:
			strcpy(out, gSubMenu_PTT_ID[gSubMenuSelection]);
			break;
		case MENU_MDF:
			strcpy(out, gSubMenu_MDF[gSubMenuSelection]);
			break;
#ifdef ENABLE_VOICE
		case MENU_VOICE:
			strcpy(out, gSubMenu_VOICE[gSubMenuSelection]);
			break;
#endif
#ifdef ENABLE_ALARM
		case MENU_AL_MOD:
			strcpy(out, gSubMenu_AL_MOD[gSubMenuSelection]);
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
				strcpy(out, gSubMenu_F_LOCK[gSubMenuSelection]);
			break;
		case MENU_RESET:
			strcpy(out, gSubMenu_RESET[gSubMenuSelection]);
			break;
		case MENU_BATTYP:
			strcpy(out, gSubMenu_BATTYP[gSubMenuSelection]);
			break;
		case MENU_SET_NAV:
			strcpy(out, gSubMenu_SET_NAV[gSubMenuSelection]);
			break;
		case MENU_F1SHRT:
		case MENU_F1LONG:
		case MENU_F2SHRT:
		case MENU_F2LONG:
		case MENU_MLONG:
			strcpy(out, gSubMenu_SIDEFUNCTIONS[gSubMenuSelection].name);
			break;
		case MENU_LIST_CH:
		case MENU_S_LIST:
			if (gSubMenuSelection == MR_CHANNELS_LIST + 1)
				strcpy(out, "ALL");
			else if (gSubMenuSelection == 0 && m == MENU_LIST_CH)
				strcpy(out, "OFF");
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
				strcpy(out, gSubMenu_OFF_ON[0]);
			else
				sprintf(out, "%u", (unsigned)gSubMenuSelection);
#else
			strcpy(out, gSubMenu_NA);
#endif
			break;
		case MENU_COMPAND:
		case MENU_ABR_ON_TX_RX:
			strcpy(out, gSubMenu_RX_TX[gSubMenuSelection]);
			break;
		case MENU_SC_REV:
			if (gSubMenuSelection == 0)
				strcpy(out, "STOP");
			else if (gSubMenuSelection < 81)
				sprintf(out, "CARRIER\n%02ds:%03dms", (int)((gSubMenuSelection * 250) / 1000), (int)((gSubMenuSelection * 250) % 1000));
			else
				sprintf(out, "TIMEOUT\n%02dm:%02ds", (int)(((gSubMenuSelection - 80) * 5) / 60), (int)(((gSubMenuSelection - 80) * 5) % 60));
			break;
		case MENU_AUTOLK:
			if (gSubMenuSelection == 0)
				strcpy(out, gSubMenu_OFF_ON[0]);
			else
				sprintf(out, "%02dm:%02ds", (int)((gSubMenuSelection * 15) / 60), (int)((gSubMenuSelection * 15) % 60));
			break;
#ifdef ENABLE_DTMF_CALLING
		case MENU_ANI_ID:
			strcpy(out, gEeprom.ANI_DTMF_ID);
			break;
		case MENU_D_RSP:
			strcpy(out, gSubMenu_D_RSP[gSubMenuSelection]);
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
				strcpy(out, gSubMenu_OFF_ON[0]);
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
			strcpy(out, gSubMenu_SCRAMBLER[gSubMenuSelection]);
			break;
#endif
#if defined(ENABLE_FEAT_F4HWN) && defined(ENABLE_FEAT_F4HWN_LOGO_SAV)
		case MENU_SET_SAV:
			strcpy(out, gSubMenu_SET_SAV[gSubMenuSelection]);
			break;
#endif
#ifdef ENABLE_FEAT_F4HWN
		case MENU_SET_PWR:
			sprintf(out, "%s\n%sW", gSubMenu_TXP[gSubMenuSelection + 1], gSubMenu_SET_PWR[gSubMenuSelection]);
			break;
		case MENU_SET_PTT:
			strcpy(out, gSubMenu_SET_PTT[gSubMenuSelection]);
			break;
		case MENU_SET_TOT:
		case MENU_SET_EOT:
			strcpy(out, gSubMenu_SET_TOT[gSubMenuSelection]);
			break;
		case MENU_SET_LCK:
			strcpy(out, gSubMenu_SET_LCK[gSubMenuSelection]);
			break;
		case MENU_SET_MET:
		case MENU_SET_GUI:
			strcpy(out, gSubMenu_SET_MET[gSubMenuSelection]);
			break;
		case MENU_TX_LOCK:
			if (TX_freq_check(gEeprom.VfoInfo[gEeprom.TX_VFO].pTX->Frequency) == 0)
				strcpy(out, "Inside\nF Lock\nPlan");
			else
				strcpy(out, gSubMenu_OFF_ON[gSubMenuSelection]);
			break;
		case MENU_VOL:
			/* Liste : apercu fixe "Syrup" ; le popup garde les pages SysInf. */
			strcpy(out, AUTHOR_STRING_2);
			break;
#ifdef ENABLE_FEAT_F4HWN_SLEEP
		case MENU_SET_OFF:
			if (gSubMenuSelection == 0)
				strcpy(out, gSubMenu_OFF_ON[0]);
			else if (gSubMenuSelection < 121)
				sprintf(out, "%dh:%02dm", (int)(gSubMenuSelection / 60), (int)(gSubMenuSelection % 60));
			break;
#endif
#ifdef ENABLE_FEAT_F4HWN_SCAN_FASTER
		case MENU_SET_SCN:
			strcpy(out, gSubMenu_SET_SCN[gSubMenuSelection]);
			break;
#endif
#ifdef ENABLE_FEAT_F4HWN_NARROWER
		case MENU_SET_NFM:
			strcpy(out, gSubMenu_SET_NFM[gSubMenuSelection]);
			break;
#endif
#ifdef ENABLE_FEAT_F4HWN_RESCUE_OPS
		case MENU_SET_KEY:
			strcpy(out, gSubMenu_SET_KEY[gSubMenuSelection]);
			break;
#endif
#ifdef ENABLE_FEAT_F4HWN_VOL
		case MENU_SET_VOL:
			if (gSubMenuSelection == 0)
				strcpy(out, gSubMenu_OFF_ON[0]);
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

	out[0] = '\0';
	if (view_pos >= gMenuListCount || out_len == 0)
		return;

	gMenuCursor = view_pos;
	MENU_ShowCurrentSetting();
	m = UI_MENU_GetCurrentMenuId();
	UI_MENU_FormatValue(m, tmp, sizeof(tmp));
	UI_MENU_FirstLine(out, tmp, out_len);

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

	tw = UI_MENU_SmallTextWidth(text);
	tx = (LCD_WIDTH > tw) ? (uint8_t)((LCD_WIDTH - tw) / 2u) : 0u;
	UI_MENU_DrawSmallAtY(text, tx, y_top);
}

/* Empile des lignes small centrees : ecart fixe gap_px, bloc centre dans [area_y0, area_y1). */
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

static void UI_MENU_DrawStackedSmall(
	const char *const *texts,
	uint8_t count,
	uint8_t area_y0,
	uint8_t area_y1,
	uint8_t gap_px)
{
	uint8_t i;

	if (count == 0 || area_y1 <= area_y0)
		return;

	for (i = 0; i < count; i++)
		UI_MENU_DrawSmallCenteredAtY(texts[i], UI_MENU_StackedLineY(i, count, area_y0, area_y1, gap_px));
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

// Liste : index (3x5 centre vertical), nom, valeur 3x5 a droite centre vertical.
static void UI_MENU_DrawNumberedMenuList(const int count, const int cursor)
{
	char          num[4];
	char          val[24];
	const uint8_t list_y0   = 16u - 3u;
	const uint8_t row_pitch = 8u + 1u;
	const uint8_t row_h     = 8u;
	const uint8_t name_x    = 14u;
	const int     visible   = 5;
	int           top;
	int           i;

	UI_PrintStringSmallBold("MENU", 0, 0, 0);

	for (uint8_t x = 0; x < LCD_WIDTH; x++)
		gFrameBuffer[1][x] = 0;
	UI_DrawLineBuffer(gFrameBuffer, 0, 8 + 2, LCD_WIDTH - 1, 8 + 2, 1);

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

		if (idx >= count)
			break;

		sprintf(num, "%u", (unsigned)(idx + 1));
		/* 3x5 = 6px de haut, centre dans la rangee 8px */
		GUI_DisplaySmallest(num, 1, (uint8_t)(y + 1u), false, true);

		UI_MENU_DrawSmallAtY(MenuList[gMenuIndices[idx]].name, name_x, y);

		UI_MENU_GetItemPreview((uint8_t)idx, val, sizeof(val));
		vw = UI_MENU_SmallestTextWidth(val);
		if (vw >= LCD_WIDTH)
			vx = 0;
		else
			vx = (uint8_t)(LCD_WIDTH - vw);

		name_max_w = (uint8_t)(name_x + UI_MENU_SmallTextWidth(MenuList[gMenuIndices[idx]].name) + 2u);
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
            uint8_t idx = menu_find_idx(cl->ids[k]);
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

        gMenuIndices[gMenuListCount++] = i;
    }
}

int32_t gSubMenuSelection;

// edit box
char    edit_original[17]; // a copy of the text before editing so that we can easily test for changes/difference
char    edit[17];
int     edit_index;
bool    edit_is_uppercase = false;

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
                UI_PrintString(MenuList[gMenuIndices[gMenuCursor + i - 1]].name, 0, 0, i * 2, 8);

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

            /* Popup : titre = grande police (comme avant), contenu plein ecran */
            menu_item_x1 = 2;
            menu_item_x2 = LCD_WIDTH - 1;
            UI_PrintString(MenuList[gMenuIndices[menu_index]].name, 0, 0, 0, 8);
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
        case MENU_SAVE:
        case MENU_TDR:
        case MENU_TOT:
        case MENU_ABR:
        case MENU_ABR_MIN:
        case MENU_ABR_MAX:
        case MENU_BAT_TXT:
        case MENU_PONMSG:
        case MENU_ROGER:
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
                const char *none = "None";
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
                    UI_MENU_DrawLines3(ch, name, (gAskForConfirmation == 1) ? "SURE?" : "WAIT!", area_y0, area_y1);
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
                    UI_MENU_DrawLines3(ch, name, (gAskForConfirmation == 1) ? "SURE?" : "WAIT!", area_y0, area_y1);
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
                    const uint8_t tw = UI_MENU_SmallTextWidth(String + i);
                    const uint8_t tx = (LCD_WIDTH > tw) ? (uint8_t)((LCD_WIDTH - tw) / 2u) : 0u;

                    UI_MENU_DrawSmallAtY(String + i, tx, (uint8_t)(text_y0 + (int)(li * line_h)));

                    while (i < len && String[i] >= 32)
                        i++;
                    while (i < len && String[i] < 32)
                        i++;
                }

                ST7565_Gauge(gaugeLine, gaugeMin, gaugeMax, gSubMenuSelection);
            }
            else if (gIsInSubMenu)
            {
                small = true;
                if (lines > 5)
                    lines = 5;
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
        char *pPrintStr = (gAskForConfirmation == 1) ? "SURE?" : "WAIT!";
        if (gIsInSubMenu)
            UI_PrintStringSmallNormal(pPrintStr, menu_item_x1, menu_item_x2, popup_line5);
        else
            UI_PrintString(pPrintStr, menu_item_x1, menu_item_x2, popup_line5, 8);
    }

    ST7565_BlitFullScreen();
}
