# Boot Logo + Boot Sound Design

**Date:** 2026-08-25  
**Status:** Approved — implementing / implemented  
**Approach:** A — Mangosteen-style POnMsg + custom PCM at `0x013000`; web UI in one「开机图片」tab

## Goal

1. Restore menu **POnMsg** (开机画面) with **None / Default / Logo**
2. Wire boot display so Default/Logo/None behave like Mangosteen (Syrup art for Default)
3. Add the missing **开机图片** web tab (JS/CSS already exist)
4. Inside that same tab, add **开机音效** upload: default = original dual triple-beep; if custom PCM present → play custom (max ~5 s)

## Non-goals

- MP3 decode on device
- Replacing OEM voice bank at `0x14C000+`
- Separate web tab for sound
- Changing Dondji logo address (`0x1FF000`) or Mangosteen messenger (`0x012000`)

## Flash layout (conflict-checked)

Checked against: Syrup, Mangosteen, Dondji, `uv-k1-k5-v3-game`, `uv-k1-k6v3-multi-system`.

| Address | Owner | Notes |
|--------:|-------|-------|
| `0x011000` | Syrup / Mangosteen | Boot **logo** (keep) |
| `0x012000` | Mangosteen | Messenger — **do not use** |
| **`0x013000`–`0x01CFFF`** | **Syrup boot sound (new)** | 40 KB / 10×4 KB sectors; unused elsewhere |
| `0x020000` | Dondji / Syrup / multi-system | Legacy CN names |
| `0x024000` | Dondji / Syrup / game | CN font |
| `0x14C000+` | OEM voice | Do not use |
| `0x1E0000` | RXTX log | Do not use |
| `0x1E8000` | game book | Do not use |
| `0x1FF000` | Dondji logo | Do not use |

### Boot sound blob (`0x013000`)

```
Offset  Size  Field
0x00    4     Magic "SYRS" (0x53 0x59 0x52 0x53)
0x04    1     Version = 1
0x05    1     Flags (reserved 0)
0x06    2     Sample rate = 8000 (LE uint16)
0x08    4     PCM byte length N (LE uint32), 1 ≤ N ≤ 40944
0x0C    4     Reserved 0
0x10    N     Unsigned 8-bit mono linear PCM @ 8 kHz
```

- Region size: `0xA000` (40960 B); header 16 B → max PCM **40944 B ≈ 5.118 s**
- Valid magic + rate + length → custom play; else → original beeps
- Clear: write zeros / erase magic (web「清除开机音」)

Document in `eeprom_compat.c` comments (direct `PY25Q16_*`, same style as logo / RXTX log).

## Firmware

### POnMsg enum (`settings.h`)

Align with Mangosteen:

```c
enum POWER_OnDisplayMode_t {
    POWER_ON_DISPLAY_MODE_NONE = 0,
    POWER_ON_DISPLAY_MODE_DEFAULT,
#ifdef ENABLE_FEAT_F4HWN_LOGO
    POWER_ON_DISPLAY_MODE_LOGO,
#endif
};
```

- Menu labels: `NONE` / `DEFAULT` / `LOGO`（中文：关闭 / 默认 / 自定义）
- Uncomment `{"POnMsg", MENU_PONMSG}` in `ui/menu.c`
- EEPROM load: accept `0 .. N-1` else fall back to **DEFAULT** (old ALL/SOUND/… ordinals not remapped)

### Welcome (`ui/welcome.c` + `main.c`)

| Mode | Display | Boot wait | Sound |
|------|---------|-----------|-------|
| None | Blank / skip | No | No |
| Default | Embedded `BITMAP_Syrup` + version + home label | Yes (until sound ends or key) | See below |
| Logo | Flash `0x011000+8` via `UI_DisplayLogo()` | Same | Same |

### Boot sound playback

- Hook where Mangosteen gates beeps (`BACKLIGHT_Sound` / startup backlight path)
- If mode is Default or Logo:
  1. Read header at `0x013000`
  2. If valid → stream PCM through existing DAC path (`driver/voice.c` style: TIM6 @ 8 kHz, DMA half/full refill); **do not** require `ENABLE_VOICE`
  3. Else → existing double `BEEP_880HZ_60MS_TRIPLE_BEEP`
- Map u8 sample → 12-bit DAC: `(sample << 4)` (or equivalent mid-scale if needed after bench)
- Boot wait: extend to cover playback (cap ~5.2 s); any key cancels wait and stops sound
- Keep `ENABLE_VOICE` **off** for syrup preset (welcome voice prompts stay disabled)

## Web flasher (`docs/`)

### One tab:「开机图片」

Restore `data-tab="logo"` / `#logo-content` (from Mangosteen HTML + existing Syrup i18n/CSS/JS).

**Section 1 — 开机图片** (existing `initBootLogoTab`):

- File → 128×64 mono → preview → upload/read @ `0x011000`
- Reminder: set POnMsg → Logo

**Section 2 — 开机音效** (new, same tab):

- Accept `audio/*` (mp3/wav/…)
- Browser decode → resample mono 8 kHz → clip/pad to ≤ 40944 B u8 PCM
- Optional local preview (Web Audio)
- Upload header + PCM to `0x013000` via existing SPI `0x051F` / `0x0521` chunk writes
- Buttons: 上传开机音 / 清除开机音（可选：读取状态显示时长）
- Hint: 未上传或清除后恢复默认两声蜂鸣；最长约 5 秒

Tab label stays **开机图片** (or「开机画面」if i18n tweak); no second top-level tab.

## Manual / i18n

- `lang.js`: strings for sound section under logo tab
- Brief note in `manual.zh.md` / `manual.en.md`: POnMsg + logo/sound flash addresses

## Acceptance

1. POnMsg visible; None/Default/Logo work on device
2. Logo tab appears; image upload/read works; Logo mode shows custom art
3. No custom sound → Default/Logo play original dual triple-beep
4. Upload MP3/WAV in logo tab → reboot plays custom ≤ ~5 s
5. Clear sound → beeps return
6. Addresses do not overlap messenger / font / voice / log / game / multi-system / Dondji logo

## Out of scope for first ship

- Menu toggle for boot sound on/off (presence of blob is the switch)
- Screensaver interaction with custom sound
- Compressing audio (ADPCM etc.)
