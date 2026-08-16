# Syrup Chinese UI / IME Design

**Date:** 2026-08-16  
**Status:** Approved for implementation planning  
**Reference firmware:** `D:\File\code\uvk1\Dondji`  
**Approach:** Port Dondji `ENABLE_CHINESE` stack into Syrup (scoped)

## Goals

1. Add a runtime **Language** setting: English / 中文 (default English).
2. When Chinese is selected, **menu titles and option values** display Chinese; English otherwise.
3. Replace channel-name entry with **Dondji-style T9 pinyin IME** so Chinese can be typed.
4. Home screen channel names can **render Chinese** (layout polish deferred to the user).

## Non-goals (this phase)

- Welcome, lock screen, scanner, FM, popup, and other non-menu UI localization
- Home-page layout redesign / EN–CN layout swap (only make glyphs draw; user adjusts UI later)
- Voice prompt CHI/ENG work as part of this feature
- Embedding the full CJK font into MCU flash

## Decisions (locked)

| Topic | Choice |
|-------|--------|
| Scope | Menus + values + ChName IME + home name rendering only |
| Font pack | Reuse Dondji SPI address, format, and `cn_font.bin` |
| Channel name payload | 15-byte UTF-8 in existing 16-byte slots |
| Default language | English |
| Implementation approach | Port Dondji `ENABLE_CHINESE` modules; wire into Syrup |

## Architecture

Compile flag `ENABLE_CHINESE` (default ON) gates the feature. Runtime `gUiLanguage` (`UI_LANGUAGE_EN` / `UI_LANGUAGE_CN`) selects strings and IME default mode.

```
Menu / keys              Settings / SPI              Draw
───────────────          ──────────────              ────
MENU_LANGUAGE     →      gUiLanguage @ 0x00A170
MenuList + SUBV   →      menu_lang / CN value tables → menu draw
ChName IME        →      UTF-8 names @ 0x004000+     → syrup_home names
Pinyin lookup     ←      cn_font @ 0x024000          → helper CJK glyphs
```

### Module map (port from Dondji, adapt to Syrup)

| Layer | Responsibility |
|-------|----------------|
| `settings` | Language load/save; font validate/read; pinyin candidates; UTF-8 channel name R/W |
| `helper` | UTF-8 decode + mixed ASCII/CJK blit APIs |
| `menu_lang.c` + `menu_sub_values_cn.*` | Chinese menu titles and option value tables |
| `ui/menu` + `app/menu` | `Lang` item, `SUBV`, ChName IME key handling and edit UI |
| `syrup_home` | Draw channel names via CJK-capable print path; **no layout change** |

## Language menu and strings

- New menu item: `Lang` / `MENU_LANGUAGE` (CN title: `显示语言`).
- Values: `English` / `中文`.
- On accept: set `gUiLanguage`, write SPI `0x00A170` immediately; menus switch without reboot.
- English short names remain in `MenuList[].name`.
- Chinese titles via `UI_MENU_GetMenuTitle(menu_id)` (port `menu_lang.c`; translate Syrup-only items).
- Option values via `SUBV(en, cn)` and parallel tables in `menu_sub_values_cn.c`.
- Category names (if enabled): bilingual tables; uncovered entries stay English.
- Boot: read `0x00A170` in `SETTINGS_InitEEPROM()`; invalid → English.
- Language byte must not overwrite Syrup settings-version storage around `0x00A160`.

When `ENABLE_CHINESE=OFF`: no `Lang` item; behavior matches current English-only firmware and ASCII T9.

## Font pack, channel names, home draw

### Font (same as Dondji)

- Base: `CN_FONT_FLASH_BASE = 0x024000`
- Contents: 12×12 bitmaps, unicode index, pinyin table, version byte (~205 KB total)
- Boot: `SETTINGS_InitCNFont()` validates version/index
- Runtime: `CNCharToIndex` → read bitmap; pinyin table feeds IME
- Pack is **not** linked into MCU image; flash `cn_font.bin` separately (same file as Dondji)

If font validation fails: firmware must not crash; ASCII still works; CJK glyphs may be blank.

### Channel names

- Keep slot layout: `0x004000 + channel × 16`
- Usable payload: **15 bytes UTF-8** (~5 Han characters)
- Remove ASCII-only (32–127) truncation on fetch/save; truncate on UTF-8 character boundaries
- Existing ≤10-byte ASCII names remain readable

### Home screen

- `syrup_home` uses CJK-capable print helpers for channel names
- **Do not** change current layout/typography strategy in this phase
- Advance X by glyph width (ASCII ~6 px, Han 12 px); clip/ellipsis if too wide
- User will adjust home UI afterward

### Address map (no font conflict)

| Data | SPI address | Size / notes |
|------|-------------|--------------|
| Language byte | `0x00A170` | 1 byte; separate from font |
| Channel names | `0x004000+` | 16 bytes/channel (15 used) |
| CN font + pinyin | `0x024000+` | ~205 KB |

## ChName IME (Dondji UX)

### Mode cycle (`#` / `KEY_F` short press)

- CN UI: `Pinyin → lower → UPPER → digit → symbol → Pinyin`
- EN UI: `lower → UPPER → digit → symbol → lower`
- Entering name edit under CN defaults to **Pinyin**

### Pinyin flow

1. Digits `2`–`9`: append T9 digit sequence (max 6) → matching syllables
2. `↑/↓`: select syllable; `MENU`: open Han candidates (up to 6 per page)
3. `1`–`6`: pick character → write 3-byte UTF-8 into edit buffer
4. SIDE1/SIDE2 or `↑/↓`: page candidates when more than 6
5. `0`: delete last T9 digit / clear candidates (Dondji **code** behavior; `*` inserts `-`, not backspace)
6. `EXIT`: clear pending pinyin/candidates if any; else delete character at cursor (CJK deletes 3 bytes)
7. `MENU` with no candidates: advance cursor; past end confirms save

Latin / digit / symbol modes keep multi-tap / character cycle aligned with Dondji, replacing Syrup’s ASCII-only T9 map.

## Acceptance criteria

- [ ] Menu can switch English / 中文; selection survives reboot
- [ ] CN: menu titles and option values Chinese; EN: English
- [ ] ChName supports pinyin input and save of UTF-8 Chinese names
- [ ] Home shows saved Chinese channel names (layout may be crude)
- [ ] Missing/invalid font pack: no crash; ASCII/numbers still usable
- [ ] `ENABLE_CHINESE=OFF`: matches pre-change English-only behavior

## Implementation notes

Suggested port order:

1. CMake `ENABLE_CHINESE` + SPI constants + `SETTINGS_*` CN APIs + boot init
2. `helper` CJK draw path
3. `gUiLanguage` + `MENU_LANGUAGE` + `menu_lang` / `SUBV` / CN value tables
4. Channel name UTF-8 load/save (15 bytes) + home draw wiring
5. Full T9 IME from Dondji `app/menu.c` MEM_NAME handlers + edit draw

Canonical UX reference: Dondji `App/app/menu.c` (`MENU_Key_0_to_9`, `MENU_Key_MENU`, `MENU_Key_UP_DOWN`, `KEY_F`); verify help docs against code for `*` vs `0`.
