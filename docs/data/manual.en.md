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

## 3. Notes

- For Quansheng **UV-K1 / UV-K5(K6) V3** only.
- Firmware flash needs BOOT; font/calib/writefreq use normal powered-on UI.
- Calibration dump/restore: this site always uses **0xB000** (512 bytes, v5 calib region).
- Font binary matches the Dondji CN font format.

## 4. More

Full docs will be expanded later. Maintainer: **BD1AHN**.
