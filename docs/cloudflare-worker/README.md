# Syrup Aircraft Radar / APRS — Cloudflare Worker

GitHub Pages 是静态站，浏览器又无法直连 adsb.fi / api.aprs.fi（CORS），因此需要本 Worker 做 JSON 代理。

- `/api/aircraft` 上游优先 adsb.fi，失败时回退 OpenSky / adsb.lol。
- `/api/aprs` 上游为 [aprs.fi](https://aprs.fi/page/api) loc 接口。需要在 Worker 里配置 API 密钥。

## Deploy

```bash
cd docs/cloudflare-worker
npx wrangler login
npx wrangler secret put APRSFI_APIKEY
npx wrangler deploy
```

密钥在 [aprs.fi 账户设置](https://aprs.fi/page/api) 中申请。本地 `docs/serve.py` 可读环境变量 `APRSFI_APIKEY`，或把密钥写在 `docs/.aprs-apikey`（不要提交）。

当前已部署地址：

`https://syrup-radar.ethanyan6.workers.dev`

然后在 `docs/js/radar.js` / `docs/js/aprs.js` 中设置对应默认地址，也可在页面里临时覆盖：

```js
window.SYRUP_AIRCRAFT_API = 'https://…/api/aircraft';
window.SYRUP_APRS_API = 'https://…/api/aprs';
```

## API

`GET /api/aircraft?lat=39.9&lon=116.4&radiusKm=80`

`GET /api/aprs?lat=39.9&lon=116.4&radiusKm=80`

`GET /api/aprs?name=BD1AHN`（官方 loc 查询，多个呼号用逗号分隔，最多 20 个）

Aircraft response shape matches `docs/serve.py` (`{ source, aircraft: [...] }`).
APRS response: `{ source: "aprs.fi", stations: [...] }`.
