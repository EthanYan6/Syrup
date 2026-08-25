# Boot Logo + Boot Sound Implementation Plan

> **For agentic workers:** Implement task-by-task. Steps use checkbox syntax.

**Goal:** Restore POnMsg (None/Default/Logo), wire boot logo + custom boot PCM (~5 s @ `0x013000`), and web「开机图片」tab with image + auto-truncated/convert/preview/upload sound.

**Architecture:** Mangosteen-style welcome modes; boot sound blob with magic `SYRS`; web converts audio in-browser to u8@8kHz before SPI write.

**Tech Stack:** C firmware (PY32F071), existing SPI USB cmds, docs Web Serial flasher.

---

### Task 1: POnMsg enum + menu restore
- [ ] Slim `POWER_OnDisplayMode_t` to None/Default/Logo
- [ ] Uncomment menu entry; update EN/CN submenus; settings load clamp

### Task 2: Welcome display branch
- [ ] `UI_DisplayWelcome` honors mode (None / Default Syrup / Logo flash)
- [ ] `main.c` skip wait when None

### Task 3: Boot sound firmware
- [ ] Header + PCM at `0x013000`; player via DAC; fallback beeps in `BACKLIGHT_Sound`
- [ ] Document in `eeprom_compat.c`

### Task 4: Web logo tab HTML
- [ ] Add tab button + `#logo-content` with image section + sound section

### Task 5: Web sound JS
- [ ] Decode → 8 kHz mono → truncate 40944 → preview play → upload/clear + hints/i18n
