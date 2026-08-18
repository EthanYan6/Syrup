<!--
  Syrup Firmware
  Copyright (c) 2026 BD1AHN
  Project: 小甜水 (Syrup)
  https://ethanyan6.github.io/Syrup/
-->

# Syrup User Manual (Short)

Web flasher: <https://ethanyan6.github.io/Syrup/>  
Repo: <https://github.com/EthanYan6/Syrup>

## 1. Browser

Use **Chrome / Edge / Opera** (Web Serial required).

## 2. Recommended order

1. **Backup calibration** (once before first flash)
2. **Flash firmware** (hold PTT while powering on → BOOT)
3. **Flash font** (`cn_font.bin` at `0x024000`, normal UI mode)
4. **Restore calibration** (this site always reads/writes **`0xB000`**)
5. **Write frequency** (as needed, normal UI)
6. **Basic info** (as needed: menu UP / DW codes and boot-screen last line, normal UI)

## 3. Notes

- For Quansheng **UV-K1 / UV-K5(K6) V3** only.
- Firmware flash needs BOOT; font/calib/writefreq/basic info use normal powered-on UI.
- Calibration dump/restore: this site always uses **0xB000** (512 bytes, v5 calib region).
- Font binary matches the Dondji CN font format.

## 4. Dual PTT

When enabled: the hardware PTT keys **CH1**, side key 1 keys **CH2**. The channel that is transmitting becomes the main (inverted) channel on the home screen, and stays selected after you unkey so you can keep operating it.

### Enable

1. Open the menu and go to **Side Key 1 Short** (or **Side Key 1 Long**).
2. Choose **PTT** and confirm. The other slot is set to PTT automatically.
3. Set **Set PTT** to **CLASSIC** (hold-to-talk). Enabling dual PTT also forces CLASSIC.

**PTT** is only listed under Side Key 1 short/long. It is not offered for Side Key 2 or MENU long-press.

Do not confuse this with **Set PTT** (CLASSIC / ONEPUSH). That menu only changes whether the **hardware PTT** is hold-to-talk or toggle; it does not enable dual PTT.

### Use

| Key | Effect |
|-----|--------|
| Hold hardware PTT | TX on CH1; CH1 inverted on home |
| Hold side key 1 | TX on CH2; CH2 inverted on home |
| Hold both | Hardware PTT wins (CH1) |
| Release | Stop TX |

While dual PTT is on, the whole side key 1 is CH2 PTT (press = TX, release = stop). Any other short/long action on that key is ignored.

### Disable

Change **Side Key 1 Short** or **Side Key 1 Long** to another action (e.g. **NONE**). Both slots become **NONE**, and you can assign them separately again.

## 5. More

Full docs will be expanded later. Maintainer: **BD1AHN**.
