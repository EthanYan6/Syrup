/**
 * Syrup Aircraft Radar — Cloudflare Worker proxy
 *
 * GitHub Pages cannot run Python; browsers cannot call adsb.fi due to CORS.
 *
 * Deploy:
 *   cd docs/cloudflare-worker
 *   npx wrangler login
 *   npx wrangler deploy
 */

const UA =
  'Mozilla/5.0 (compatible; SyrupRadar/1.1; +https://ethanyan6.github.io/Syrup/)';

/** Short cache to avoid hammering upstream (adsb.fi public limit is 1 req/s). */
const cache = new Map();
const CACHE_TTL_MS = 45000;

async function fetchUpstream(url, timeoutMs = 18000) {
  const ctrl = new AbortController();
  const timer = setTimeout(function () {
    ctrl.abort();
  }, timeoutMs);
  try {
    return await fetch(url, {
      signal: ctrl.signal,
      headers: {
        'User-Agent': UA,
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
    'Access-Control-Allow-Headers': 'Content-Type',
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

export default {
  async fetch(request) {
    if (request.method === 'OPTIONS') {
      return new Response(null, { status: 204, headers: corsHeaders() });
    }
    if (request.method !== 'GET') {
      return jsonResponse({ error: 'method_not_allowed' }, 405);
    }
    const url = new URL(request.url);
    if (url.pathname === '/api/aircraft' || url.pathname === '/') {
      if (url.pathname === '/' && !url.searchParams.has('lat')) {
        return jsonResponse({ ok: true, service: 'syrup-radar', path: '/api/aircraft' });
      }
      return handleAircraft(url);
    }
    return jsonResponse({ error: 'not_found' }, 404);
  },
};
