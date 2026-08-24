# Syrup Aircraft Radar — Cloudflare Worker

GitHub Pages 是静态站，浏览器又无法直连 adsb.fi（CORS），因此需要本 Worker 做同源外的 JSON 代理。上游优先 adsb.fi，失败时回退 OpenSky / adsb.lol。

## Deploy

```bash
cd docs/cloudflare-worker
npx wrangler login
npx wrangler deploy
```

记下输出的 URL，例如：

`https://syrup-radar.<subdomain>.workers.dev`

然后在 `docs/js/radar.js` 中设置：

```js
var DEFAULT_REMOTE_AIRCRAFT_API = 'https://syrup-radar.<subdomain>.workers.dev/api/aircraft';
```

也可在页面里临时覆盖：

```js
window.SYRUP_AIRCRAFT_API = 'https://…/api/aircraft';
```

## API

`GET /api/aircraft?lat=39.9&lon=116.4&radiusKm=80`

Response shape matches `docs/serve.py` (`{ source, aircraft: [...] }`).
