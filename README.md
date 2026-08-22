<div align="center">

# 🥤 Syrup（小甜水）

> Firmware for UV-K1 / UV-K5·K6 V3 (PY32F071 MCU)  
> 开源对讲机固件 · 基于 F4HWN Fusion 的 Syrup 定制版

<p>
  <a href="./docs/data/manual.zh.md">🇨🇳 使用手册</a> |
  <a href="./docs/data/manual.en.md">🇺🇸 User Manual</a>
</p>

<p>
  <a href="https://github.com/EthanYan6/Syrup/stargazers">
    <img src="https://img.shields.io/github/stars/EthanYan6/Syrup?style=flat-square" alt="Stars" />
  </a>
  <a href="https://github.com/EthanYan6/Syrup/network">
    <img src="https://img.shields.io/github/forks/EthanYan6/Syrup?style=flat-square" alt="Forks" />
  </a>
  <a href="https://img.shields.io/github/downloads/EthanYan6/Syrup/total">
    <img src="https://img.shields.io/github/downloads/EthanYan6/Syrup/total?style=flat-square" alt="Downloads" />
  </a>
  <a href="https://github.com/EthanYan6/Syrup/releases">
    <img src="https://img.shields.io/github/v/release/EthanYan6/Syrup?style=flat-square" alt="Release" />
  </a>
  <a href="https://github.com/EthanYan6/Syrup/issues">
    <img src="https://img.shields.io/github/issues/EthanYan6/Syrup?style=flat-square" alt="Issues" />
  </a>
  <img src="https://komarev.com/ghpvc/?username=EthanYan6&repo=Syrup&style=flat-square" alt="Repo views" />
</p>

![Repobeats](https://repobeats.axiom.co/api/embed/ecdd86aa536b716f088339a0c5ee734558f78c28.svg "Repobeats analytics")

</div>

---

## Maintainer

**BD1AHN**

## Official Website

https://ethanyan6.github.io/Syrup/

## Current build

- Brand / author string: **Syrup**
- Version: **v1.1.2**（`syrup` CMake preset）
- Target: Quansheng **UV-K1 / UV-K5(K6) V3**（PY32F071）

---

## ✨ Syrup 特色功能

相对上游 F4HWN Fusion，小甜水重点打磨了日常使用体验与网页工具链：

- **中英双语界面** — 菜单可切换语言；全量 GB2312 字库刷入 SPI Flash
- **主页双行 UI** — CH1 / CH2 常显，反色标记主信道；波形 / S 表 / 上次接收跟听跟收
- **双 PTT** — 硬件 PTT 打 CH1，侧键 1 打 CH2，发射行反色并保持主信道
- **三守** — 主页三行轮询 CH1 / CH2 / CH3；硬件 PTT / 侧键 1 / 侧键 2 分别发射，与双 PTT 互斥
- **接收模式说明清晰** — 仅主信道 / 双频守候 / 跨段 / 主发双收 / 三守，听与打的规则与主页显示一致
- **Yan ID** — 松开 PTT 后发 FSK 呼号（最多 6 位）；对方主页显示来电 ID；兼容叮咚鸡 / mangosteen GGM2
- **飞机雷达** — 网页 ADS-B 扫描 → 串口推送到手台；机内 AM 空管收听；数字键输频（`*` 作小数点）；目标掉电保存
- **导航键自适应** — 跟随菜单 **SetNav**（UV-K1 左/右，UV-K5 上/下），含飞机页步进
- **浏览器刷机站** — 固件 / 字库 / 校准（`0xB000`）/ 写频 / 基础信息 / 飞机雷达，一站完成
- **中英文使用手册** — 站点内嵌，覆盖刷机顺序、双 PTT、接收模式、三守、Yan ID、飞机雷达按键

仍保留 Fusion 常见能力（按 `syrup` 预设开启）：频谱、广播 FM、VOX、AirCopy、BEAM、Fox Hunt、游戏、扫描增强、RF Log、自定义开机画面等。

---

## 🌐 刷机与手册

| 入口 | 链接 |
|------|------|
| 在线刷机 | https://ethanyan6.github.io/Syrup/ |
| 中文手册 | [docs/data/manual.zh.md](./docs/data/manual.zh.md) |
| English manual | [docs/data/manual.en.md](./docs/data/manual.en.md) |
| Releases | https://github.com/EthanYan6/Syrup/releases |

本地预览站点（飞机雷达也可走 Cloudflare Worker，GitHub Pages 同样可用）：

```bash
python docs/serve.py
```

GitHub Pages 上飞机雷达依赖 Cloudflare Worker 代理（`docs/cloudflare-worker`）。若飞机刷不出来，请确认 Worker 仍在线；首次部署：

```bash
cd docs/cloudflare-worker
npx wrangler login
npx wrangler deploy
```

部署后把 `docs/js/radar.js` 里的 `DEFAULT_REMOTE_AIRCRAFT_API` 改成你的 `*.workers.dev/api/aircraft` 地址。

### 推荐刷机顺序

1. 备份校准  
2. BOOT 刷固件  
3. 正常开机刷字库（`0x024000`）  
4. 恢复校准（本站固定 `0xB000`）  
5. 按需写频 / 基础信息  

仅适用于 **UV-K1 / UV-K5(K6) V3**。刷机有风险，请自行判断。

---

## 🔧 编译（Docker）

```bash
# 构建镜像（首次）
docker build -t uvk1-uvk5v3 .

# 配置并编译 syrup 预设
docker run --rm -v "$PWD:/src" -w /src uvk1-uvk5v3 \
  bash -c 'cmake --preset syrup && cmake --build --preset syrup -j$(nproc)'
```

产物会同步到 `docs/firmware/syrup.bin`。

Windows（PowerShell）示例：

```powershell
docker run --rm -v "${PWD}:/src" -w /src uvk1-uvk5v3 `
  bash -c 'cmake --preset syrup && cmake --build --preset syrup -j$(nproc)'
```

---

## 📦 Upstream

本仓库基于 [F4HWN / UV-K1·K5V3 custom firmware](https://github.com/armel/uv-k1-k5v3-firmware-custom)（Fusion），并继承 Egzumer / DualTachyon 等开源工作。上游完整功能说明与捐赠名单见其 README。

同作者相关项目：[Dondji（叮咚鸡）](https://github.com/EthanYan6/Dondji)

---

## License

Copyright 2023 Dual Tachyon and contributors.  
Syrup project identity maintained by **BD1AHN**.

Licensed under the Apache License, Version 2.0.  
See [LICENSE](./LICENSE) if present, or <http://www.apache.org/licenses/LICENSE-2.0>.

> **WARNING** — Use at your own risk. This firmware may brick your radio. Always back up calibration before flashing.
