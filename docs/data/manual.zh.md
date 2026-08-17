<!--
  Syrup Firmware
  Copyright (c) 2026 BD1AHN
  Project: 小甜水 (Syrup)
  https://ethanyan6.github.io/Syrup/
-->

# 小甜水使用手册（简版）

官方刷机页：<https://ethanyan6.github.io/Syrup/>  
仓库：<https://github.com/EthanYan6/Syrup>

## 1. 浏览器要求

请使用 **Chrome / Edge / Opera**（需 Web Serial）。

## 2. 推荐刷机顺序

1. **备份校准**（原厂或首次刷入前备份一次即可）
2. **刷固件**（关机后按住 PTT 开机进入 BOOT）
3. **刷字库**（正常开机界面下刷入 `cn_font.bin`，地址 `0x024000`）
4. **恢复校准**（固定写入 **v5 校准位 `0xB000`**，不按版本切换）
5. **写频 / 开机图片**（按需，正常开机界面）

## 3. 注意事项

- 仅适用于泉盛 **UV-K1 / UV-K5(K6) V3**。老版本 K5/K6 可能变砖。
- 刷固件需 BOOT 模式；刷字库、校准、写频、开机图均在**正常使用界面**下连接 USB。
- 校准备份/恢复固定使用 **0xB000**（512 字节）。
- 字库与叮咚鸡同源格式，有更新时请重新刷入。

## 4. 更多

完整功能说明与常见问题后续补充。维护者：**BD1AHN**。
