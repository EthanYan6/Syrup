# FM Radio Page: Band Label + RX EQ Bar

Date: 2026-08-16  
Status: Implemented (approach A)

## Goal

On the FM radio screen (`UI_DisplayFM`):

1. Move the band range text (e.g. `64.5-108M`) from the bottom to the **top-right**, drawn with the **smallest** font (`GUI_DisplaySmallest` / `gFont3x5`).
2. Below the `VFO(CHxx)` / `MR(CHxx)` line, leave a **2px** gap, then draw a **Syrup-home-style RX EQ pillar** bar at the bottom of the content area.
3. Pillars respond to FM signal strength (and related dynamics), with the same visual language as the main Syrup home RX wave: chaotic EQ columns, silence freezes body and lets the peak tip fall.

## Non-goals

- Do not refactor Syrup home into a shared EQ library (rejected approach B).
- Do not call Syrup home tick/draw directly (rejected approach C).
- Do not change frequency digit layout, `FM` title, or SAVE/DEL/scan copy.
- Do not show the EQ bar during SAVE?, DEL?, or scan modes.

## Current layout constraints

- Content framebuffer: `FRAME_LINES = 7` → **56px** tall (`gFrameBuffer[0..6]`), plus separate status line.
- Existing FM content:
  - Lines 0–1: `FM` (big font)
  - Lines 1–2: frequency digits
  - Lines 3–4: mode string `VFO` / `VFO(CHxx)` / `MR(CHxx)` (big font, ends at content y=39)
  - Line 6: band range (to be removed from here)
- Free space under VFO: content y **40–55** = **16px**.
- With **2px** gap after VFO: bar starts at y **42**, ends at y **55** → height **14px**.

**Decision:** Keep upper layout; EQ height = **14px** (not home’s 20px).

## UI layout (target)

```
[status bar — unchanged]
+----------------------------------------------+
| FM                          64.5-108M        |  line 0 (band: smallest, right)
|           108.0                              |  lines 1–2 frequency
|                                              |
|              VFO(CH01)                       |  lines 3–4
|  (2px gap)                                   |
|  ████ ▄█ ████ ▄▄ … EQ pillars …             |  y 42–55 (14px)
+----------------------------------------------+
```

### Band label

- Format unchanged: `sprintf("%d%s-%dM", lo/10, band0?".5":"", hi/10)`.
- Font: `GUI_DisplaySmallest(..., statusbar=false, fill=true)`.
- Position: right-aligned in the top content row. Glyph advance is 4px per character; compute `x = LCD_WIDTH - (len * 4)` (clamp ≥ 0). Vertical: `y ≈ 1` within framebuffer so it sits in line 0 without colliding with `FM` on the left (`FM` starts at x=2).
- Draw on every full `UI_DisplayFM` refresh.
- Remove `UI_PrintStringSmallNormal(..., line 6)` band draw.

### EQ bar visibility

Show and animate only when **all** are true:

- `gScreenToDisplay == DISPLAY_FM`
- `gFM_ScanState == FM_SCAN_OFF`
- `!gAskToSave && !gAskToDelete`
- Normal listen path that currently prints `VFO` / `VFO(CHxx)` / `MR(CHxx)` (not A-SCAN / M-SCAN)

Otherwise: do not draw pillars; leave bottom clear (existing SAVE/DEL/scan strings remain as today).

## Visual / animation design

Mirror Syrup home RX pillars, scaled to 14px:

| Constant (home) | FM value |
|-----------------|----------|
| Wave height | 14 |
| Block width | 5 |
| Column pitch | 6 (5 + 1 gap) |
| Max blocks | `14 / 3` → **4** (pack slots evenly into 14px like home) |
| Columns | `LCD_WIDTH / 6` → **21** |
| Tip fall period | same feel as home (~5 ticks) |
| Tick period | ~50ms (`5 × 10ms`) |

Behavior:

- While “active” (signal dynamics above gate): per-column pseudo-random mix from phase/col, slow attack/release (±1 block/tick), tip tracks peak.
- Below gate / weak lock: body clears immediately; tip falls one step every tip-fall period (home `sample_rx_decay`).

Drawing uses framebuffer pixel helpers already used by home (`UI_DrawPixelBuffer` / local fill_rect on `gFrameBuffer`), then blit **lines 5–6** only when possible to avoid full-screen flicker.

## Signal source (BK1080)

FM uses BK1080, not BK4819. Map chip status into a 0..255-ish `activity` analogous to home’s AF/glitch/noise mix:

1. Read `BK1080_REG_10` → `rssi = BK1080_REG_10_GET_RSSI(...)` (0–255).
2. Read `BK1080_REG_07` → `snr = BK1080_REG_07_GET_SNR(...)` (0–15).
3. Optional: AFCRL bit — if railed, treat as weaker / paused (same as scan lock helpers already distrust railed AFCRL).
4. Compose dynamics, e.g. `activity = rssi + (snr << 3)` (clamp 255). Tune gate so:

   - Tuned station with speech/music → lively pillars
   - Dead air / very weak RSSI (scan threshold historically `< 10`) → decay
   - Mute path if FM mute is active → decay

Exact scale constants are implementation-tuned once on device; keep them as named `#define`s next to the FM EQ code.

**Note:** BK1080 does not expose a BK4819-style voice amplitude for RX audio. RSSI+SNR (plus small phase noise for column chaos) is the approved stand-in for “signal strength / sound presence” on this page.

## Code structure (approach A)

Keep logic self-contained under FM UI:

| File | Change |
|------|--------|
| `App/ui/fmradio.c` | Band relocate; EQ sample/draw; full paint hooks |
| `App/ui/fmradio.h` | Declare `UI_FM_Tick10ms(void)` (or similar) under `ENABLE_FMRADIO` |
| `App/app/app.c` | Call `UI_FM_Tick10ms()` near `UI_SyrupHome_Tick10ms()` (no-op unless `DISPLAY_FM`) |

Optional tiny static helpers inside `fmradio.c` only (no new `.c` unless flash/size forces it):

- `fm_eq_reset`, `fm_eq_sample`, `fm_eq_draw`, `fm_eq_blit`
- `fm_band_label_draw`

Reset pillar state when leaving FM mode or when visibility becomes false (scan/SAVE/DEL), so re-entry does not show stale tips.

## Data flow

```
APP 10ms loop
  → UI_FM_Tick10ms()
       if not DISPLAY_FM or not listen-visible: maybe decay once / return
       every 5 ticks:
         read BK1080 RSSI/SNR → activity
         sample columns (attack/release or decay)
         clear y42–55, draw pillars
         ST7565_BlitLine(5); ST7565_BlitLine(6);

UI_DisplayFM()
  → clear, draw FM / frequency / mode
  → draw band smallest top-right
  → if listen-visible: draw current EQ frame (state preserved across ticks)
  → BlitFullScreen
```

## Error / edge cases

- Low battery / keypad lock overlays: if FM page already shows unlock hints on a content line, do not overwrite that line with EQ; if unlock uses line 5 today, prefer skipping EQ while hint visible (match home’s “overlay freezes wave” idea).
- Input-box frequency entry: still “listen-visible” if not SAVE/DEL/scan — keep EQ.
- Channel string modes that replace frequency with `CH-xx` (delete/save): EQ hidden per visibility rules.

## Testing

- Enter FM: band appears top-right; bottom EQ animates on a strong station.
- Weak/no station: pillars decay to empty (tips fall).
- Start scan / SAVE? / DEL?: EQ disappears; mode text unchanged.
- Exit FM to main: no leftover FM tick work; home EQ unaffected.
- Confirm no overlap: band vs `FM`, EQ vs `VFO` (2px gap), EQ stays in lines 5–6.
- Visual parity: column width/gap and tip-fall feel recognizable vs Syrup home (shorter height OK).

## Implementation notes

- Prefer blit lines 5–6 on tick; full blit only on `UI_DisplayFM`.
- Do not commit secrets or unrelated files; scope is FM UI + one app tick call.
- No design-doc commit required by repo policy unless the user asks.
