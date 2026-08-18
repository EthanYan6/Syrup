# Dual PTT Design

## Summary

When **Side Key 1 Short** or **Side Key 1 Long** is set to **PTT**, the radio enters dual-PTT mode:

- Hardware PTT → transmit on **CH1 (VFO A)**, main channel switches to CH1
- Side Key 1 (hold) → transmit on **CH2 (VFO B)**, main channel switches to CH2
- If both are held, hardware PTT wins (CH1)
- After TX ends, the active main channel remains on the channel that was transmitting

## Menu Rules

- **PTT** appears only in **Side Key 1 Short** and **Side Key 1 Long** action lists
- **Side Key 2** and **MENU Long** lists skip the PTT entry
- Selecting PTT in either Side Key 1 slot sets **both** slots to PTT
- Changing away from PTT in either Side Key 1 slot sets **both** slots to **None**
- Legacy configs with PTT on Side Key 2 or MENU Long are migrated to **None** on load

## Side Key 1 Behavior

When dual PTT is enabled, the entire Side Key 1 acts as hold-to-talk for CH2 (press = TX, release = stop). Short/long action semantics are bypassed.

## Hardware PTT

Classic / one-push mode (`Set PTT` menu) still applies to the hardware PTT only. Side Key 1 always uses hold-to-talk.

## Key Lock

When lock scope allows PTT, Side Key 1 dual PTT is treated like hardware PTT.

## Implementation

- `ACTION_DualPttEnabled()`, `ACTION_SetMainVfo()`, `ACTION_HandleSide1Ptt()` in `action.c`
- Hardware PTT path in `CheckKeys()` / `GENERIC_Key_PTT()`
- Menu filtering and sync in `menu.c`
- Settings migration in `settings.c`
