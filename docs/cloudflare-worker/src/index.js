import { connect } from 'cloudflare:sockets';

/**
 * Syrup Aircraft Radar / APRS — Cloudflare Worker proxy
 *
 * GitHub Pages cannot run Python; browsers cannot call adsb.fi / api.aprs.fi (CORS).
 *
 * Deploy:
 *   cd docs/cloudflare-worker
 *   npx wrangler login
 *   npx wrangler deploy
 */

const UA =
  'Mozilla/5.0 (compatible; SyrupRadar/1.1; +https://ethanyan6.github.io/Syrup/)';
const APRS_UA =
  'Mozilla/5.0 (compatible; SyrupAprs/1.0; +https://ethanyan6.github.io/Syrup/)';

/** Short cache to avoid hammering upstream (adsb.fi public limit is 1 req/s). */
const cache = new Map();
const CACHE_TTL_MS = 45000;

async function fetchUpstream(url, timeoutMs = 18000, userAgent = UA) {
  const ctrl = new AbortController();
  const timer = setTimeout(function () {
    ctrl.abort();
  }, timeoutMs);
  try {
    return await fetch(url, {
      signal: ctrl.signal,
      headers: {
        'User-Agent': userAgent,
        Accept: 'application/json',
        'Accept-Language': 'en-US,en;q=0.9',
      },
      cf: { cacheTtl: 0, cacheEverything: false },
    });
  } finally {
    clearTimeout(timer);
  }
}

function corsHeaders() {
  return {
    'Access-Control-Allow-Origin': '*',
    'Access-Control-Allow-Methods': 'GET, OPTIONS',
    'Access-Control-Allow-Headers': 'Content-Type, X-APRS-ApiKey, Authorization',
    'Cache-Control': 'no-store',
  };
}

function jsonResponse(obj, status = 200) {
  return new Response(JSON.stringify(obj), {
    status,
    headers: {
      'Content-Type': 'application/json; charset=utf-8',
      ...corsHeaders(),
    },
  });
}

function kmToLatDelta(km) {
  return km / 111.32;
}

function kmToLonDelta(km, lat) {
  let cos = Math.cos((lat * Math.PI) / 180);
  if (Math.abs(cos) < 0.01) cos = 0.01;
  return km / (111.32 * cos);
}

function cacheKey(lat, lon, radiusKm) {
  return `${lat.toFixed(2)},${lon.toFixed(2)},${Math.round(radiusKm)}`;
}

function getCached(key) {
  const hit = cache.get(key);
  if (!hit) return null;
  if (Date.now() > hit.expires) {
    cache.delete(key);
    return null;
  }
  return hit.payload;
}

function setCached(key, payload) {
  cache.set(key, { expires: Date.now() + CACHE_TTL_MS, payload });
  if (cache.size > 64) {
    const first = cache.keys().next().value;
    cache.delete(first);
  }
}

async function fetchOpensky(lat, lon, radiusKm) {
  const dLat = kmToLatDelta(radiusKm);
  const dLon = kmToLonDelta(radiusKm, lat);
  const qs = new URLSearchParams({
    lamin: (lat - dLat).toFixed(6),
    lomin: (lon - dLon).toFixed(6),
    lamax: (lat + dLat).toFixed(6),
    lomax: (lon + dLon).toFixed(6),
  });
  const res = await fetchUpstream(
    `https://opensky-network.org/api/states/all?${qs}`,
    22000
  );
  if (res.status === 429) throw new Error('opensky_http_429');
  if (!res.ok) throw new Error(`opensky_http_${res.status}`);
  const data = await res.json();
  const aircraft = [];
  for (const row of data.states || []) {
    if (!row || row.length < 11) continue;
    const lonV = row[5];
    const latV = row[6];
    if (latV == null || lonV == null) continue;
    const alt = row[13] != null ? row[13] : row[7];
    aircraft.push({
      icao: String(row[0] || '').trim(),
      callsign: String(row[1] || '').trim(),
      lat: latV,
      lon: lonV,
      alt_m: typeof alt === 'number' ? alt : null,
      velocity_ms: typeof row[9] === 'number' ? row[9] : null,
      track: typeof row[10] === 'number' ? row[10] : null,
      on_ground: !!row[8],
    });
  }
  return { source: 'opensky', aircraft };
}

async function fetchAdsbLol(lat, lon, radiusKm) {
  const radiusNm = Math.max(1, Math.min(250, Math.round(radiusKm / 1.852)));
  const url = `https://api.adsb.lol/v2/lat/${lat.toFixed(5)}/lon/${lon.toFixed(5)}/dist/${radiusNm}`;
  const res = await fetchUpstream(url, 15000);
  if (res.status === 429) throw new Error('adsblol_http_429');
  if (!res.ok) throw new Error(`adsblol_http_${res.status}`);
  return normalizeAdsbAc(await res.json(), 'adsb.lol');
}

async function fetchAdsbFi(lat, lon, radiusKm) {
  const radiusNm = Math.max(1, Math.min(250, Math.round(radiusKm / 1.852)));
  const url = `https://opendata.adsb.fi/api/v3/lat/${lat.toFixed(5)}/lon/${lon.toFixed(5)}/dist/${radiusNm}`;
  const res = await fetchUpstream(url, 15000);
  if (res.status === 403) throw new Error('adsbfi_http_403');
  if (res.status === 429) throw new Error('adsbfi_http_429');
  if (!res.ok) throw new Error(`adsbfi_http_${res.status}`);
  return normalizeAdsbAc(await res.json(), 'adsb.fi');
}

function normalizeAdsbAc(data, source) {
  const aircraft = [];
  for (const ac of data.ac || []) {
    if (ac.lat == null || ac.lon == null) continue;
    let alt = ac.alt_geom != null ? ac.alt_geom : ac.alt_baro;
    let alt_m = null;
    if (typeof alt === 'number' && alt > -1000) alt_m = alt * 0.3048;
    let velocity_ms = null;
    if (typeof ac.gs === 'number') velocity_ms = ac.gs / 1.94384;
    aircraft.push({
      icao: String(ac.hex || '').trim(),
      callsign: String(ac.flight || '').trim(),
      lat: ac.lat,
      lon: ac.lon,
      alt_m,
      velocity_ms,
      track: typeof ac.track === 'number' ? ac.track : null,
      on_ground: !!ac.ground || alt === 'ground',
    });
  }
  return { source, aircraft };
}

async function handleAircraft(url) {
  const lat = parseFloat(url.searchParams.get('lat') || '');
  const lon = parseFloat(url.searchParams.get('lon') || '');
  let radiusKm = parseFloat(url.searchParams.get('radiusKm') || '80');
  if (!Number.isFinite(lat) || !Number.isFinite(lon)) {
    return jsonResponse({ error: 'invalid_params' }, 400);
  }
  if (lat < -90 || lat > 90 || lon < -180 || lon > 180) {
    return jsonResponse({ error: 'invalid_coords' }, 400);
  }
  radiusKm = Math.max(5, Math.min(250, Number.isFinite(radiusKm) ? radiusKm : 80));

  const key = cacheKey(lat, lon, radiusKm);
  const cached = getCached(key);
  if (cached) {
    return jsonResponse({ ...cached, cached: true });
  }

  /* adsb.fi first; OpenSky/adsb.lol if fi 403's Cloudflare IPs. */
  const errors = [];
  for (const fetcher of [fetchAdsbFi, fetchOpensky, fetchAdsbLol]) {
    try {
      const payload = await fetcher(lat, lon, radiusKm);
      setCached(key, payload);
      return jsonResponse(payload);
    } catch (e) {
      errors.push(String(e && e.message ? e.message : e));
    }
  }
  return jsonResponse({ error: 'upstream_failed', detail: errors }, 502);
}

function toNum(value) {
  if (value == null || value === '') return null;
  const n = Number(value);
  return Number.isFinite(n) ? n : null;
}

function normalizeAprs(data) {
  const stations = [];
  for (const row of data.entries || []) {
    if (!row) continue;
    if (String(row.type || '') === 'a') continue;
    const lat = toNum(row.lat);
    const lon = toNum(row.lng);
    if (lat == null || lon == null) continue;
    const name = String(row.name || row.srccall || '').trim();
    if (!name) continue;
    stations.push({
      name,
      srccall: String(row.srccall || '').trim(),
      type: String(row.type || 'l'),
      lat,
      lon,
      comment: String(row.comment || ''),
      speed_kmh: toNum(row.speed),
      course: toNum(row.course),
      altitude_m: toNum(row.altitude),
      lasttime: toNum(row.lasttime) != null ? Math.round(toNum(row.lasttime)) : null,
      symbol: String(row.symbol || ''),
      path: String(row.path || ''),
      phg: String(row.phg || ''),
    });
  }
  return { source: 'aprs.fi', stations };
}

function dmToDeg(deg, minutes, hemi) {
  let d = Number(deg) + Number(minutes) / 60;
  if (hemi === 'S' || hemi === 'W') d = -d;
  return d;
}

function decodeBase91(s) {
  let n = 0;
  for (let i = 0; i < s.length; i++) n = n * 91 + (s.charCodeAt(i) - 33);
  return n;
}

function parseUncompressedPos(body) {
  const m = String(body || '').match(
    /^(\d{2})(\d{2}\.\d+)([NS])(.)(\d{3})(\d{2}\.\d+)([EW])(.)/
  );
  if (!m) return null;
  return {
    lat: dmToDeg(m[1], m[2], m[3]),
    lon: dmToDeg(m[5], m[6], m[7]),
    symbol: m[4] + m[8],
    rest: body.slice(m[0].length),
  };
}

function parseCompressedPos(body) {
  if (!body || body.length < 13) return null;
  const table = body.charAt(0);
  if (table !== '/' && table !== '\\') return null;
  const ys = body.slice(1, 5);
  const xs = body.slice(5, 9);
  for (let i = 0; i < 8; i++) {
    const c = (i < 4 ? ys : xs).charCodeAt(i % 4);
    if (c < 33 || c > 123) return null;
  }
  let course = null;
  let speed_kmh = null;
  /* csT: T bits 0x10 → course/speed (APRS 1.01 §9) */
  const tbits = (body.charCodeAt(12) - 33) & 0x18;
  if (tbits === 0x10) {
    let cse = (body.charCodeAt(10) - 33) * 4;
    if (cse >= 360) cse = 0;
    course = cse;
    speed_kmh = (Math.pow(1.08, body.charCodeAt(11) - 33) - 1) * 1.852;
  }
  return {
    lat: 90 - decodeBase91(ys) / 380926,
    lon: -180 + decodeBase91(xs) / 190463,
    symbol: table + body.charAt(9),
    rest: body.slice(13),
    course,
    speed_kmh,
  };
}

function parseAprsInfo(info) {
  if (!info) return null;
  const t = info.charAt(0);
  let start = 0;
  if (t === '!' || t === '=') start = 1;
  else if (t === '/' || t === '@') start = 8;
  else if (t === ';') start = 18;
  else return null;
  if (info.length <= start) return null;
  const body = info.slice(start);
  return parseUncompressedPos(body) || parseCompressedPos(body);
}

function parseAprsPacket(line) {
  if (!line || line.charAt(0) === '#') return null;
  const gt = line.indexOf('>');
  const col = line.indexOf(':');
  if (gt < 1 || col <= gt) return null;
  const src = line.slice(0, gt).trim();
  if (!src) return null;
  const info = line.slice(col + 1);
  const pos = parseAprsInfo(info);
  if (!pos || !Number.isFinite(pos.lat) || !Number.isFinite(pos.lon)) return null;
  if (pos.lat < -90 || pos.lat > 90 || pos.lon < -180 || pos.lon > 180) return null;
  let comment = String(pos.rest || '');
  let course = typeof pos.course === 'number' && Number.isFinite(pos.course) ? pos.course : null;
  let speed = typeof pos.speed_kmh === 'number' && Number.isFinite(pos.speed_kmh) ? pos.speed_kmh : null;
  const cs = comment.match(/^(\d{3})\/(\d{3})(.*)$/);
  if (cs) {
    course = Number(cs[1]);
    speed = Number(cs[2]) * 1.852;
    comment = cs[3] || '';
  }
  comment = comment.replace(/^[\s>/]*/, '').slice(0, 80);
  return {
    name: src,
    srccall: src,
    type: info.charAt(0) === ';' ? 'o' : 'l',
    lat: pos.lat,
    lon: pos.lon,
    comment,
    speed_kmh: Number.isFinite(speed) ? speed : null,
    course: Number.isFinite(course) ? course : null,
    altitude_m: null,
    lasttime: Math.round(Date.now() / 1000),
    symbol: pos.symbol || '',
    path: line.slice(gt + 1, col),
    phg: '',
  };
}

const APRS_IS_HOSTS = ['rotate.aprs2.net', 'euro.aprs2.net'];

function withTimeout(promise, ms, label) {
  let timer;
  return Promise.race([
    promise.finally(function () {
      clearTimeout(timer);
    }),
    new Promise(function (_, reject) {
      timer = setTimeout(function () {
        reject(new Error(label || 'timeout'));
      }, ms);
    }),
  ]);
}

async function fetchAprsIsFrom(host, lat, lon, radiusKm) {
  const filter = `r/${lat.toFixed(4)}/${lon.toFixed(4)}/${Math.round(radiusKm)}`;
  const login = `user BD1AHN-TS pass -1 vers SyrupAprs 1.0 filter ${filter}\r\n`;
  const socket = connect({ hostname: host, port: 14580 });
  const writer = socket.writable.getWriter();
  const reader = socket.readable.getReader();
  const encoder = new TextEncoder();
  const decoder = new TextDecoder();
  try {
    await writer.write(encoder.encode(login));
    let buf = '';
    const deadline = Date.now() + 6000;
    while (Date.now() < deadline) {
      const remain = deadline - Date.now();
      const result = await Promise.race([
        reader.read(),
        new Promise(function (resolve) {
          setTimeout(function () {
            resolve({ timeout: true });
          }, remain);
        }),
      ]);
      if (result.timeout || result.done) break;
      buf += decoder.decode(result.value || new Uint8Array(), { stream: true });
    }
    const seen = new Map();
    buf.split(/\r?\n/).forEach(function (line) {
      const st = parseAprsPacket(line.trim());
      if (st) seen.set(st.name, st);
    });
    return { source: 'aprs-is', stations: Array.from(seen.values()) };
  } finally {
    try {
      writer.releaseLock();
    } catch (e) {}
    try {
      reader.releaseLock();
    } catch (e) {}
    try {
      socket.close();
    } catch (e) {}
  }
}

async function fetchAprsIsNearby(lat, lon, radiusKm) {
  let lastErr;
  for (let i = 0; i < APRS_IS_HOSTS.length; i++) {
    try {
      return await withTimeout(
        fetchAprsIsFrom(APRS_IS_HOSTS[i], lat, lon, radiusKm),
        10000,
        'aprs_is_timeout'
      );
    } catch (e) {
      lastErr = e;
    }
  }
  throw lastErr || new Error('aprs_is_failed');
}

async function handleAprs(url, env, request) {
  const name = String(url.searchParams.get('name') || '').trim();
  const lat = parseFloat(url.searchParams.get('lat') || '');
  const lon = parseFloat(url.searchParams.get('lon') || '');
  let radiusKm = parseFloat(url.searchParams.get('radiusKm') || '80');
  const hasLat = Number.isFinite(lat);
  const hasLon = Number.isFinite(lon);
  if (hasLat !== hasLon) {
    return jsonResponse({ error: 'invalid_params' }, 400);
  }
  if (hasLat && (lat < -90 || lat > 90 || lon < -180 || lon > 180)) {
    return jsonResponse({ error: 'invalid_coords' }, 400);
  }
  if (!name && !hasLat) {
    return jsonResponse({ error: 'invalid_params' }, 400);
  }
  radiusKm = Math.max(5, Math.min(250, Number.isFinite(radiusKm) ? radiusKm : 80));

  const headerKey = request
    ? String(request.headers.get('X-APRS-ApiKey') || '').trim()
    : '';
  const queryKey = String(
    url.searchParams.get('apikey') || url.searchParams.get('key') || ''
  ).trim();
  const apikey =
    headerKey || queryKey || String((env && env.APRSFI_APIKEY) || '').trim();

  const cacheName = name || '';
  const key = cacheName
    ? `n:${cacheName.toUpperCase()}`
    : cacheKey(lat, lon, radiusKm);
  const cached = getCached(`aprs:${key}`);
  if (cached) {
    return jsonResponse({ ...cached, cached: true });
  }

  if (!name && hasLat) {
    try {
      const payload = await fetchAprsIsNearby(lat, lon, radiusKm);
      setCached(`aprs:${key}`, payload);
      return jsonResponse(payload);
    } catch (e) {
      return jsonResponse(
        { error: 'upstream_failed', detail: [String(e && e.message ? e.message : e)] },
        502
      );
    }
  }

  if (!apikey) {
    return jsonResponse({ error: 'missing_apikey' }, 503);
  }

  const params = new URLSearchParams({
    what: 'loc',
    apikey,
    format: 'json',
    name,
  });

  try {
    const res = await fetchUpstream(
      `https://api.aprs.fi/api/get?${params}`,
      22000,
      APRS_UA
    );
    if (res.status === 429) {
      return jsonResponse({ error: 'rate_limited' }, 429);
    }
    if (!res.ok) {
      return jsonResponse({ error: 'upstream_failed', detail: [`aprsfi_http_${res.status}`] }, 502);
    }
    const data = await res.json();
    if (!data || data.result !== 'ok') {
      const desc = String((data && (data.description || data.result)) || 'fail');
      return jsonResponse({ error: 'upstream_failed', detail: [`aprsfi_${desc}`] }, 502);
    }
    const payload = normalizeAprs(data);
    setCached(`aprs:${key}`, payload);
    return jsonResponse(payload);
  } catch (e) {
    return jsonResponse(
      { error: 'upstream_failed', detail: [String(e && e.message ? e.message : e)] },
      502
    );
  }
}

export default {
  async fetch(request, env) {
    if (request.method === 'OPTIONS') {
      return new Response(null, { status: 204, headers: corsHeaders() });
    }
    if (request.method !== 'GET') {
      return jsonResponse({ error: 'method_not_allowed' }, 405);
    }
    const url = new URL(request.url);
    if (url.pathname === '/api/aprs') {
      return handleAprs(url, env, request);
    }
    if (url.pathname === '/api/aircraft' || url.pathname === '/') {
      if (url.pathname === '/' && !url.searchParams.has('lat') && !url.searchParams.has('name')) {
        return jsonResponse({
          ok: true,
          service: 'syrup-radar',
          paths: ['/api/aircraft', '/api/aprs'],
          aprsNearby: 'aprs-is',
        });
      }
      return handleAircraft(url);
    }
    return jsonResponse({ error: 'not_found' }, 404);
  },
};
