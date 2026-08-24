/*
 * Syrup Firmware
 *
 * Copyright (c) 2026 BD1AHN
 *
 * Licensed under the Apache License, Version 2.0
 *
 * Project:
 *     小甜水 (Syrup)
 *
 * Official Website:
 *     https://ethanyan6.github.io/Syrup/
 *
 * APRS map tab: browser geolocation + aprs.fi loc API (web-only).
 */
(function () {
  'use strict';

  var POLL_MS = 20000;
  var AUTO_PUSH_IDLE_MS = 60000;
  var AUTO_PUSH_INTERVAL_MS = 10000;
  /**
   * APRS JSON endpoints (tried in order).
   * 1) Local docs/serve.py → /api/aprs
   * 2) Optional override: window.SYRUP_APRS_API
   * 3) Cloudflare Worker (for GitHub Pages; deploy docs/cloudflare-worker)
   */
  var DEFAULT_REMOTE_APRS_API = 'https://syrup-radar.ethanyan6.workers.dev/api/aprs';
  var APIKEY_STORAGE = 'syrupAprsFiApiKey';

  function aprsEndpoints() {
    var list = ['/api/aprs'];
    if (typeof window.SYRUP_APRS_API === 'string' && window.SYRUP_APRS_API) {
      list.push(window.SYRUP_APRS_API);
    }
    list.push(DEFAULT_REMOTE_APRS_API);
    return list;
  }

  var state = {
    active: false,
    paused: false,
    userLat: null,
    userLon: null,
    stations: [],
    selectedName: null,
    pollTimer: null,
    animFrame: null,
    sweepAngle: 0,
    lastAnimTs: 0,
    fetching: false,
    retryAfterMs: 0,
    geoAsked: false,
    lastQueryName: '',
    missingKey: false,
    manualOverride: false,
    autoPushIndex: 0,
    idleTimer: null,
    autoTimer: null,
    autoPushing: false
  };

  function $(id) {
    return document.getElementById(id);
  }

  var RADIUS_STORAGE_KEY = 'syrup-aprs-radius-km';

  function currentRadiusKm() {
    var el = $('aprsRadiusInput');
    var n = parseFloat(el && el.value);
    if (!isFinite(n)) return 80;
    return Math.max(5, Math.min(250, n));
  }

  function restoreRadius(el) {
    if (!el) return;
    try {
      var saved = localStorage.getItem(RADIUS_STORAGE_KEY);
      if (!saved) return;
      if (el.querySelector('option[value="' + saved + '"]')) {
        el.value = saved;
      }
    } catch (e) {}
  }

  function persistRadius(km) {
    try {
      localStorage.setItem(RADIUS_STORAGE_KEY, String(km));
    } catch (e) {}
  }

  function tr(key, params) {
    if (typeof window.t === 'function') {
      return window.t(key, params || {});
    }
    return key;
  }

  function isAprsTabActive() {
    var tab = document.querySelector('.tab.active');
    return !!(tab && tab.dataset.tab === 'aprs');
  }

  function toRad(deg) {
    return (deg * Math.PI) / 180;
  }

  function toDeg(rad) {
    return (rad * 180) / Math.PI;
  }

  function distanceKm(lat1, lon1, lat2, lon2) {
    var R = 6371;
    var dLat = toRad(lat2 - lat1);
    var dLon = toRad(lon2 - lon1);
    var a =
      Math.sin(dLat / 2) * Math.sin(dLat / 2) +
      Math.cos(toRad(lat1)) * Math.cos(toRad(lat2)) *
      Math.sin(dLon / 2) * Math.sin(dLon / 2);
    return R * 2 * Math.atan2(Math.sqrt(a), Math.sqrt(1 - a));
  }

  function bearingDeg(lat1, lon1, lat2, lon2) {
    var φ1 = toRad(lat1);
    var φ2 = toRad(lat2);
    var Δλ = toRad(lon2 - lon1);
    var y = Math.sin(Δλ) * Math.cos(φ2);
    var x = Math.cos(φ1) * Math.sin(φ2) - Math.sin(φ1) * Math.cos(φ2) * Math.cos(Δλ);
    return (toDeg(Math.atan2(y, x)) + 360) % 360;
  }

  function setStatus(key, params) {
    var el = $('aprsStatus');
    if (!el) return;
    el.textContent = tr(key, params);
    el.setAttribute('data-i18n', key);
    if (params) {
      el.setAttribute('data-i18n-params', JSON.stringify(params));
    } else {
      el.removeAttribute('data-i18n-params');
    }
  }

  function updateMeta() {
    var countEl = $('aprsCount');
    var updatedEl = $('aprsUpdated');
    if (countEl) {
      countEl.textContent = tr('aprsCount', { n: state.stations.length });
      countEl.removeAttribute('data-i18n');
    }
    if (updatedEl) {
      if (state.lastUpdated) {
        updatedEl.textContent = tr('aprsUpdated', { time: state.lastUpdated });
      } else {
        updatedEl.textContent = tr('aprsUpdatedNever');
      }
      updatedEl.removeAttribute('data-i18n');
    }
  }

  function formatAlt(m) {
    if (m == null || !isFinite(m)) return '—';
    var ft = Math.round(m * 3.28084);
    return Math.round(m) + ' m / ' + ft + ' ft';
  }

  function formatSpeed(kmh) {
    if (kmh == null || !isFinite(kmh)) return '—';
    var kt = Math.round(kmh / 1.852);
    return Math.round(kmh) + ' km/h / ' + kt + ' kt';
  }

  function formatCourse(deg) {
    if (deg == null || !isFinite(deg)) return '—';
    return Math.round(deg) + '°';
  }

  function formatDist(km) {
    if (km == null || !isFinite(km)) return '—';
    if (km < 10) return km.toFixed(1) + ' km';
    return Math.round(km) + ' km';
  }

  function formatHeard(unix) {
    if (unix == null || !isFinite(unix)) return '—';
    var sec = Math.max(0, Math.floor(Date.now() / 1000 - unix));
    if (sec < 60) return tr('aprsHeardJustNow');
    if (sec < 3600) return tr('aprsHeardMinutes', { n: Math.floor(sec / 60) });
    if (sec < 86400) return tr('aprsHeardHours', { n: Math.floor(sec / 3600) });
    return tr('aprsHeardDays', { n: Math.floor(sec / 86400) });
  }

  function typeLabel(type) {
    var key = {
      l: 'aprsTypeStation',
      o: 'aprsTypeObject',
      i: 'aprsTypeItem',
      w: 'aprsTypeWx',
      a: 'aprsTypeAis'
    }[type];
    return key ? tr(key) : (type || '—');
  }

  function findStation(name) {
    var i;
    var want = String(name || '').toUpperCase();
    for (i = 0; i < state.stations.length; i++) {
      if (String(state.stations[i].name).toUpperCase() === want) {
        return state.stations[i];
      }
    }
    return null;
  }

  function selectStation(name, opts) {
    state.selectedName = name || null;
    var empty = $('aprsDetailEmpty');
    var fields = $('aprsDetailFields');
    var st = findStation(name);
    var pushToRadio = !(opts && opts.skipPush);
    if (!st) {
      if (empty) empty.hidden = false;
      if (fields) fields.hidden = true;
      renderList();
      return;
    }
    if (empty) empty.hidden = true;
    if (fields) fields.hidden = false;
    var map = {
      aprsFieldCallsign: st.name || '—',
      aprsFieldType: typeLabel(st.type),
      aprsFieldComment: st.comment || '—',
      aprsFieldSpeed: formatSpeed(st.speedKmh),
      aprsFieldCourse: formatCourse(st.course),
      aprsFieldAlt: formatAlt(st.alt),
      aprsFieldDist: formatDist(st.distanceKm),
      aprsFieldBrg: formatCourse(st.bearing),
      aprsFieldHeard: formatHeard(st.lasttime),
      aprsFieldPath: st.path || '—'
    };
    Object.keys(map).forEach(function (id) {
      var el = $(id);
      if (el) el.textContent = map[id];
    });
    renderList();
    if (pushToRadio) {
      pushSelectedToRadio(st, { fromAuto: !!(opts && opts.fromAuto) });
    }
    updateSerialButtons();
  }

  function updateSerialButtons() {
    var api = window.SyrupSerial;
    var connected = !!(api && api.isConnected && api.isConnected());
    var connectBtn = $('aprsConnectBtn');
    var pushBtn = $('aprsPushBtn');
    if (connectBtn) {
      connectBtn.textContent = connected
        ? tr('aprsDisconnect')
        : tr('aprsConnect');
      connectBtn.setAttribute('data-i18n', connected ? 'aprsDisconnect' : 'aprsConnect');
    }
    if (pushBtn) {
      pushBtn.disabled = !connected || !state.selectedName;
    }
  }

  function isSerialConnected() {
    var api = window.SyrupSerial;
    return !!(api && api.isConnected && api.isConnected());
  }

  function pushSelectedToRadio(st, opts) {
    var api = window.SyrupSerial;
    var fromAuto = !!(opts && opts.fromAuto);
    if (!api || typeof api.pushAprs !== 'function') {
      return;
    }
    if (!api.isConnected || !api.isConnected()) {
      setStatus('aprsStatusNeedSerial');
      updateSerialButtons();
      return;
    }
    if (!fromAuto) {
      noteUserPush();
    }
    var label = (st && st.name) || '';
    var altM = (st && typeof st.alt === 'number' && isFinite(st.alt)) ? Math.round(st.alt) : -2147483648;
    var distM = (st && typeof st.distanceKm === 'number' && isFinite(st.distanceKm))
      ? Math.min(65534, Math.round(st.distanceKm * 1000))
      : 0xFFFF;
    api.pushAprs({
      callsign: label,
      altitudeM: altM,
      distanceM: distM,
      course: (st && typeof st.course === 'number' && isFinite(st.course)) ? Math.round(st.course) : -1,
      speedKmh: (st && typeof st.speedKmh === 'number' && isFinite(st.speedKmh)) ? Math.round(st.speedKmh) : -1,
      openPage: true
    }).then(function () {
      setStatus(fromAuto ? 'aprsStatusAutoPushed' : 'aprsStatusPushed', {
        call: String(label || '').trim() || '—'
      });
      updateSerialButtons();
    }).catch(function (err) {
      console.warn('aprs push', err);
      setStatus('aprsStatusPushFail');
      updateSerialButtons();
    });
  }

  function sortedStations() {
    return state.stations.slice().sort(function (a, b) {
      var da = (a.distanceKm == null || !isFinite(a.distanceKm)) ? 1e9 : a.distanceKm;
      var db = (b.distanceKm == null || !isFinite(b.distanceKm)) ? 1e9 : b.distanceKm;
      return da - db;
    });
  }

  function clearAutoPushTimers() {
    if (state.idleTimer) {
      clearTimeout(state.idleTimer);
      state.idleTimer = null;
    }
    if (state.autoTimer) {
      clearInterval(state.autoTimer);
      state.autoTimer = null;
    }
    state.autoPushing = false;
  }

  /** Connected: round-robin every 10s. First tick waits one interval. */
  function startAutoPushLoop() {
    clearAutoPushTimers();
    if (!state.active || !isSerialConnected()) return;
    state.autoPushing = true;
    state.autoTimer = setInterval(autoPushNext, AUTO_PUSH_INTERVAL_MS);
  }

  /** Manual push: wait 1 minute, then resume 10s auto-push. */
  function noteUserPush() {
    state.autoPushIndex = 0;
    clearAutoPushTimers();
    if (!state.active || !isSerialConnected()) return;
    state.idleTimer = setTimeout(beginAutoPush, AUTO_PUSH_IDLE_MS);
  }

  function beginAutoPush() {
    state.idleTimer = null;
    if (!state.active || !isSerialConnected()) return;
    state.autoPushing = true;
    autoPushNext();
    state.autoTimer = setInterval(autoPushNext, AUTO_PUSH_INTERVAL_MS);
  }

  function autoPushNext() {
    if (!state.active || !state.autoPushing) return;
    if (!isSerialConnected()) {
      clearAutoPushTimers();
      return;
    }
    var sorted = sortedStations();
    if (!sorted.length) return;
    if (state.autoPushIndex >= sorted.length) {
      state.autoPushIndex = 0;
    }
    var st = sorted[state.autoPushIndex];
    state.autoPushIndex += 1;
    selectStation(st.name, { fromAuto: true });
  }

  function renderList() {
    var list = $('aprsList');
    if (!list) return;
    list.innerHTML = '';
    var sorted = sortedStations();
    sorted.forEach(function (st) {
      var li = document.createElement('li');
      li.className = 'radar-list-item' + (st.name === state.selectedName ? ' is-selected' : '');
      li.setAttribute('role', 'button');
      li.tabIndex = 0;
      li.dataset.name = st.name;
      var metaBits = [formatDist(st.distanceKm), formatCourse(st.bearing), typeLabel(st.type)];
      li.innerHTML =
        '<span class="radar-list-call">' + escapeHtml(st.name) + '</span>' +
        '<span class="radar-list-meta">' + escapeHtml(metaBits.join(' · ')) + '</span>';
      li.addEventListener('click', function () {
        selectStation(st.name);
      });
      li.addEventListener('keydown', function (ev) {
        if (ev.key === 'Enter' || ev.key === ' ') {
          ev.preventDefault();
          selectStation(st.name);
        }
      });
      list.appendChild(li);
    });
  }

  function escapeHtml(s) {
    return String(s)
      .replace(/&/g, '&amp;')
      .replace(/</g, '&lt;')
      .replace(/>/g, '&gt;')
      .replace(/"/g, '&quot;');
  }

  function parseStationsPayload(payload) {
    var out = [];
    if (!payload || !payload.stations || !Array.isArray(payload.stations)) {
      return out;
    }
    payload.stations.forEach(function (row) {
      if (!row) return;
      var lat = row.lat;
      var lon = row.lon;
      if (lat == null || lon == null || !isFinite(lat) || !isFinite(lon)) return;
      var name = String(row.name || '').trim();
      if (!name) return;
      var dist = null;
      var brg = null;
      if (state.userLat != null && state.userLon != null) {
        dist = distanceKm(state.userLat, state.userLon, lat, lon);
        brg = bearingDeg(state.userLat, state.userLon, lat, lon);
        if (!state.lastQueryName && dist > currentRadiusKm() * 1.05) return;
      }
      out.push({
        name: name,
        srccall: String(row.srccall || '').trim(),
        type: String(row.type || 'l'),
        lat: lat,
        lon: lon,
        comment: String(row.comment || ''),
        alt: typeof row.altitude_m === 'number' ? row.altitude_m : null,
        speedKmh: typeof row.speed_kmh === 'number' ? row.speed_kmh : null,
        course: typeof row.course === 'number' ? row.course : null,
        lasttime: typeof row.lasttime === 'number' ? row.lasttime : null,
        symbol: String(row.symbol || ''),
        path: String(row.path || ''),
        distanceKm: dist,
        bearing: brg
      });
    });
    return out;
  }

  function currentSearchValue() {
    var el = $('aprsSearch');
    return el ? String(el.value || '').trim() : '';
  }

  function readStoredApiKey() {
    try {
      return String(window.localStorage.getItem(APIKEY_STORAGE) || '').trim();
    } catch (e) {
      return '';
    }
  }

  function fillApiKeyInput() {
    var el = $('aprsApiKeyInput');
    if (!el) return;
    el.value = readStoredApiKey();
  }

  function currentApiKey() {
    var el = $('aprsApiKeyInput');
    var typed = el ? String(el.value || '').trim() : '';
    return typed || readStoredApiKey();
  }

  function saveApiKeyFromInput() {
    var el = $('aprsApiKeyInput');
    var key = el ? String(el.value || '').trim() : '';
    try {
      if (key) {
        window.localStorage.setItem(APIKEY_STORAGE, key);
      } else {
        window.localStorage.removeItem(APIKEY_STORAGE);
      }
    } catch (e) {
      console.warn('aprs apikey storage', e);
    }
    if (el) el.value = key;
    state.missingKey = false;
    state.retryAfterMs = 0;
    if (!key) {
      setStatus('aprsStatusKeyCleared');
      return;
    }
    setStatus('aprsStatusKeySaved');
    if (!state.active) return;
    state.fetching = false;
    var name = currentSearchValue();
    if (name) {
      fetchStations({ name: name });
      return;
    }
    if (state.userLat != null) {
      startPoll();
    } else {
      requestGeoThenStart();
    }
  }

  function fetchHeaders() {
    var headers = {};
    var key = currentApiKey();
    if (key) {
      headers['X-APRS-ApiKey'] = key;
    }
    return headers;
  }

  function fetchStations(opts) {
    if (!state.active || state.fetching) return;
    if (document.visibilityState === 'hidden') return;
    var name = opts && typeof opts.name === 'string' ? String(opts.name).trim() : '';
    var nearby = !name;
    if (nearby && state.paused) return;
    if (nearby && (state.userLat == null || state.userLon == null)) return;

    var now = Date.now();
    if (state.retryAfterMs && now < state.retryAfterMs) {
      setStatus('aprsStatusRateLimited');
      return;
    }

    var params = new URLSearchParams();
    if (name) {
      params.set('name', name.toUpperCase());
      state.lastQueryName = name.toUpperCase();
    } else {
      params.set('lat', String(state.userLat));
      params.set('lon', String(state.userLon));
      params.set('radiusKm', String(currentRadiusKm()));
      state.lastQueryName = '';
    }

    var endpoints = aprsEndpoints();
    var qi = params.toString();

    state.fetching = true;
    setStatus('aprsStatusFetching');

    function tryAt(index) {
      if (index >= endpoints.length) {
        if (state.missingKey) {
          setStatus('aprsStatusNeedKey');
        } else {
          setStatus('aprsStatusError');
        }
        state.fetching = false;
        return;
      }
      var base = endpoints[index];
      fetch(base + (base.indexOf('?') >= 0 ? '&' : '?') + qi, {
        cache: 'no-store',
        headers: fetchHeaders()
      })
        .then(function (res) {
          if (res.status === 429) {
            state.retryAfterMs = Date.now() + 30000;
            throw new Error('rate_limited');
          }
          if (res.status === 503) {
            return res.json().then(function (data) {
              if (data && data.error === 'missing_apikey') {
                state.missingKey = true;
                throw new Error('missing_apikey');
              }
              throw new Error('try_next');
            }).catch(function (err) {
              if (err && (err.message === 'missing_apikey' || err.message === 'try_next')) {
                throw err;
              }
              throw new Error('try_next');
            });
          }
          if (res.status === 404 || res.status === 502) {
            throw new Error('try_next');
          }
          if (!res.ok) {
            throw new Error('http_' + res.status);
          }
          return res.json().then(function (data) {
            if (data && data.error) {
              if (data.error === 'missing_apikey') {
                state.missingKey = true;
                throw new Error('missing_apikey');
              }
              throw new Error('try_next');
            }
            return data;
          });
        })
        .then(function (data) {
          state.retryAfterMs = 0;
          state.missingKey = false;
          state.stations = parseStationsPayload(data);
          var d = new Date();
          state.lastUpdated =
            String(d.getHours()).padStart(2, '0') + ':' +
            String(d.getMinutes()).padStart(2, '0') + ':' +
            String(d.getSeconds()).padStart(2, '0');
          updateMeta();
          if (state.selectedName) {
            selectStation(state.selectedName, { skipPush: true });
          } else {
            renderList();
          }
          if (state.stations.length === 0) {
            setStatus(name ? 'aprsStatusEmptySearch' : 'aprsStatusEmpty');
          } else {
            setStatus('aprsStatusOk', { n: state.stations.length });
          }
          state.fetching = false;
        })
        .catch(function (err) {
          if (err && err.message === 'rate_limited') {
            setStatus('aprsStatusRateLimited');
            state.fetching = false;
            return;
          }
          if (err && err.message === 'missing_apikey') {
            setStatus('aprsStatusNeedKey');
            state.fetching = false;
            return;
          }
          if (err && (err.message === 'try_next' || err.message === 'Failed to fetch' ||
              err.name === 'TypeError')) {
            tryAt(index + 1);
            return;
          }
          console.warn('aprs fetch', base, err);
          tryAt(index + 1);
        });
    }

    tryAt(0);
  }

  function clearPoll() {
    if (state.pollTimer) {
      clearInterval(state.pollTimer);
      state.pollTimer = null;
    }
  }

  function startPoll() {
    clearPoll();
    if (!state.active || state.paused) return;
    fetchStations({});
    state.pollTimer = setInterval(function () {
      fetchStations({});
    }, POLL_MS);
  }

  function fillCoordInputs() {
    var latEl = $('aprsLatInput');
    var lonEl = $('aprsLonInput');
    if (latEl && state.userLat != null && isFinite(state.userLat)) {
      latEl.value = Number(state.userLat).toFixed(4);
    }
    if (lonEl && state.userLon != null && isFinite(state.userLon)) {
      lonEl.value = Number(state.userLon).toFixed(4);
    }
  }

  function applyManualCoords() {
    var latEl = $('aprsLatInput');
    var lonEl = $('aprsLonInput');
    var lat = parseFloat(latEl && latEl.value);
    var lon = parseFloat(lonEl && lonEl.value);
    if (!isFinite(lat) || !isFinite(lon) || lat < -90 || lat > 90 || lon < -180 || lon > 180) {
      setStatus('radarStatusBadCoords');
      return;
    }
    var search = $('aprsSearch');
    if (search) search.value = '';
    state.manualOverride = true;
    state.userLat = lat;
    state.userLon = lon;
    state.retryAfterMs = 0;
    fillCoordInputs();
    if (!state.active) {
      state.active = true;
      startAnim();
    }
    if (state.paused) {
      setPaused(false);
    } else {
      startPoll();
    }
  }

  function requestGeoThenStart(force) {
    if (!navigator.geolocation) {
      setStatus('aprsStatusNoGeo');
      return;
    }
    setStatus('aprsStatusLocating');
    state.geoAsked = true;
    navigator.geolocation.getCurrentPosition(
      function (pos) {
        if (state.manualOverride && !force) return;
        if (force) state.manualOverride = false;
        state.userLat = pos.coords.latitude;
        state.userLon = pos.coords.longitude;
        state.retryAfterMs = 0;
        fillCoordInputs();
        setStatus('aprsStatusFetching');
        if (!state.active) {
          state.active = true;
          startAnim();
        }
        if (state.paused) {
          setPaused(false);
        } else {
          startPoll();
        }
      },
      function () {
        if (state.manualOverride && !force) return;
        setStatus('aprsStatusGeoDenied');
      },
      { enableHighAccuracy: false, timeout: 15000, maximumAge: force ? 0 : 60000 }
    );
  }

  function drawMap(ts) {
    var canvas = $('aprsCanvas');
    if (!canvas) return;
    var ctx = canvas.getContext('2d');
    if (!ctx) return;

    var dpr = window.devicePixelRatio || 1;
    var cssSize = Math.min(canvas.clientWidth || 420, 480);
    var size = Math.max(280, cssSize);
    if (canvas.width !== Math.round(size * dpr) || canvas.height !== Math.round(size * dpr)) {
      canvas.width = Math.round(size * dpr);
      canvas.height = Math.round(size * dpr);
    }
    ctx.setTransform(dpr, 0, 0, dpr, 0, 0);

    var w = size;
    var h = size;
    var cx = w / 2;
    var cy = h / 2;
    var r = Math.min(w, h) * 0.46;

    if (!state.lastAnimTs) state.lastAnimTs = ts;
    var dt = Math.min(50, ts - state.lastAnimTs);
    state.lastAnimTs = ts;
    state.sweepAngle = (state.sweepAngle + dt * 0.09) % 360;

    var dark = document.documentElement.classList.contains('theme-dark');
    ctx.clearRect(0, 0, w, h);

    var grad = ctx.createRadialGradient(cx, cy, r * 0.1, cx, cy, r);
    if (dark) {
      grad.addColorStop(0, '#163044');
      grad.addColorStop(1, '#0a1520');
    } else {
      grad.addColorStop(0, '#1a3a52');
      grad.addColorStop(1, '#0c1c28');
    }
    ctx.beginPath();
    ctx.arc(cx, cy, r, 0, Math.PI * 2);
    ctx.fillStyle = grad;
    ctx.fill();
    ctx.strokeStyle = dark ? '#5aa7d4' : '#6cb8e0';
    ctx.lineWidth = 2;
    ctx.stroke();

    ctx.strokeStyle = 'rgba(120, 200, 230, 0.28)';
    ctx.lineWidth = 1;
    [0.33, 0.66, 1].forEach(function (f) {
      ctx.beginPath();
      ctx.arc(cx, cy, r * f, 0, Math.PI * 2);
      ctx.stroke();
    });

    ctx.beginPath();
    ctx.moveTo(cx - r, cy);
    ctx.lineTo(cx + r, cy);
    ctx.moveTo(cx, cy - r);
    ctx.lineTo(cx, cy + r);
    ctx.stroke();

    ctx.fillStyle = 'rgba(180, 220, 240, 0.55)';
    ctx.font = '11px Nunito, sans-serif';
    ctx.textAlign = 'left';
    ctx.fillText(Math.round(currentRadiusKm() / 3) + ' km', cx + 4, cy - r * 0.33 + 4);
    ctx.fillText(Math.round((currentRadiusKm() * 2) / 3) + ' km', cx + 4, cy - r * 0.66 + 4);
    ctx.fillText(currentRadiusKm() + ' km', cx + 4, cy - r + 12);

    var ang = toRad(state.sweepAngle - 90);
    ctx.save();
    ctx.beginPath();
    ctx.moveTo(cx, cy);
    ctx.arc(cx, cy, r, ang - 0.35, ang, false);
    ctx.closePath();
    var sweepGrad = ctx.createRadialGradient(cx, cy, 0, cx, cy, r);
    sweepGrad.addColorStop(0, 'rgba(120, 210, 255, 0.35)');
    sweepGrad.addColorStop(1, 'rgba(120, 210, 255, 0.02)');
    ctx.fillStyle = sweepGrad;
    ctx.fill();
    ctx.restore();

    ctx.beginPath();
    ctx.moveTo(cx, cy);
    ctx.lineTo(cx + Math.cos(ang) * r, cy + Math.sin(ang) * r);
    ctx.strokeStyle = 'rgba(160, 230, 255, 0.9)';
    ctx.lineWidth = 2;
    ctx.stroke();

    ctx.beginPath();
    ctx.arc(cx, cy, 4, 0, Math.PI * 2);
    ctx.fillStyle = '#ffd54a';
    ctx.fill();

    state.stations.forEach(function (st) {
      if (st.distanceKm == null || !isFinite(st.distanceKm) || st.bearing == null) return;
      var frac = Math.min(1, st.distanceKm / currentRadiusKm());
      var brg = toRad(st.bearing - 90);
      var px = cx + Math.cos(brg) * r * frac;
      var py = cy + Math.sin(brg) * r * frac;
      var selected = st.name === state.selectedName;
      var wx = st.type === 'w';
      ctx.beginPath();
      if (wx) {
        ctx.rect(px - (selected ? 4.5 : 3), py - (selected ? 4.5 : 3), selected ? 9 : 6, selected ? 9 : 6);
      } else {
        ctx.arc(px, py, selected ? 5 : 3.5, 0, Math.PI * 2);
      }
      ctx.fillStyle = selected ? '#ffd54a' : (wx ? '#7ec8ff' : '#7dffc8');
      ctx.fill();
      if (selected) {
        ctx.strokeStyle = 'rgba(255, 213, 74, 0.8)';
        ctx.lineWidth = 2;
        ctx.stroke();
      }
      if (st.name && (selected || state.stations.length < 25)) {
        ctx.fillStyle = 'rgba(220, 245, 255, 0.9)';
        ctx.font = '10px Nunito, sans-serif';
        ctx.textAlign = 'left';
        ctx.fillText(st.name.slice(0, 10), px + 6, py + 3);
      }
    });
  }

  function animLoop(ts) {
    drawMap(ts || performance.now());
    state.animFrame = requestAnimationFrame(animLoop);
  }

  function startAnim() {
    if (state.animFrame) return;
    state.lastAnimTs = 0;
    state.animFrame = requestAnimationFrame(animLoop);
  }

  function stopAnim() {
    if (state.animFrame) {
      cancelAnimationFrame(state.animFrame);
      state.animFrame = null;
    }
  }

  function setPaused(paused) {
    state.paused = !!paused;
    var btn = $('aprsPauseBtn');
    if (btn) {
      btn.textContent = state.paused ? tr('aprsResume') : tr('aprsPause');
      btn.setAttribute('data-i18n', state.paused ? 'aprsResume' : 'aprsPause');
    }
    if (state.paused) {
      clearPoll();
      setStatus('aprsStatusPaused');
    } else if (state.active && state.userLat != null) {
      startPoll();
    }
  }

  function runSearch() {
    var name = currentSearchValue();
    if (!name) {
      if (state.userLat != null) {
        startPoll();
      } else {
        requestGeoThenStart();
      }
      return;
    }
    clearPoll();
    fetchStations({ name: name });
  }

  function activate() {
    state.active = true;
    startAnim();
    if (state.userLat == null) {
      requestGeoThenStart();
    } else if (!state.paused) {
      startPoll();
    } else {
      setStatus('aprsStatusPaused');
    }
    if (isSerialConnected() && !state.idleTimer && !state.autoTimer) {
      startAutoPushLoop();
    }
  }

  function deactivate() {
    state.active = false;
    clearPoll();
    stopAnim();
  }

  function onTabMaybeChanged() {
    if (isAprsTabActive()) {
      activate();
    } else {
      deactivate();
    }
  }

  function hitTestCanvas(ev) {
    var canvas = $('aprsCanvas');
    if (!canvas || !state.stations.length) return;
    var rect = canvas.getBoundingClientRect();
    var x = ((ev.clientX - rect.left) / rect.width) * (canvas.clientWidth || rect.width);
    var y = ((ev.clientY - rect.top) / rect.height) * (canvas.clientHeight || rect.height);
    var size = Math.min(canvas.clientWidth || 420, 480);
    size = Math.max(280, size);
    var cx = size / 2;
    var cy = size / 2;
    var r = Math.min(size, size) * 0.46;
    var best = null;
    var bestD = 14;
    state.stations.forEach(function (st) {
      if (st.distanceKm == null || st.bearing == null) return;
      var frac = Math.min(1, st.distanceKm / currentRadiusKm());
      var brg = toRad(st.bearing - 90);
      var px = cx + Math.cos(brg) * r * frac;
      var py = cy + Math.sin(brg) * r * frac;
      var dx = px - x;
      var dy = py - y;
      var d = Math.sqrt(dx * dx + dy * dy);
      if (d < bestD) {
        bestD = d;
        best = st;
      }
    });
    if (best) selectStation(best.name);
  }

  function bindUi() {
    var refreshBtn = $('aprsRefreshBtn');
    var pauseBtn = $('aprsPauseBtn');
    var connectBtn = $('aprsConnectBtn');
    var pushBtn = $('aprsPushBtn');
    var searchBtn = $('aprsSearchBtn');
    var search = $('aprsSearch');
    var canvas = $('aprsCanvas');
    var coordQueryBtn = $('aprsCoordQueryBtn');
    var myLocBtn = $('aprsUseMyLocationBtn');
    var radiusSelect = $('aprsRadiusInput');
    var latInput = $('aprsLatInput');
    var lonInput = $('aprsLonInput');
    if (refreshBtn) {
      refreshBtn.addEventListener('click', function () {
        if (!state.active) return;
        state.retryAfterMs = 0;
        var name = currentSearchValue();
        if (name) {
          fetchStations({ name: name });
          return;
        }
        if (state.userLat == null) {
          requestGeoThenStart();
          return;
        }
        fetchStations({});
      });
    }
    if (pauseBtn) {
      pauseBtn.addEventListener('click', function () {
        if (!state.active) return;
        setPaused(!state.paused);
      });
    }
    if (searchBtn) {
      searchBtn.addEventListener('click', function () {
        if (!state.active) return;
        runSearch();
      });
    }
    if (search) {
      search.addEventListener('keydown', function (ev) {
        if (ev.key === 'Enter') {
          ev.preventDefault();
          runSearch();
        }
      });
    }
    if (connectBtn) {
      connectBtn.addEventListener('click', function () {
        var api = window.SyrupSerial;
        if (!api) return;
        if (api.isConnected && api.isConnected()) {
          Promise.resolve(api.disconnect && api.disconnect()).finally(function () {
            clearAutoPushTimers();
            updateSerialButtons();
            setStatus('aprsStatusIdle');
          });
          return;
        }
        Promise.resolve(api.connect && api.connect())
          .then(function () {
            setStatus('aprsStatusConnected');
            updateSerialButtons();
            startAutoPushLoop();
          })
          .catch(function (err) {
            console.warn('aprs connect', err);
            setStatus('aprsStatusNeedSerial');
            updateSerialButtons();
          });
      });
    }
    if (pushBtn) {
      pushBtn.addEventListener('click', function () {
        if (!state.selectedName) return;
        var st = findStation(state.selectedName);
        if (!st) return;
        pushSelectedToRadio(st);
      });
    }
    if (canvas) {
      canvas.addEventListener('click', hitTestCanvas);
    }
    if (coordQueryBtn) {
      coordQueryBtn.addEventListener('click', applyManualCoords);
    }
    if (myLocBtn) {
      myLocBtn.addEventListener('click', function () {
        var search = $('aprsSearch');
        if (search) search.value = '';
        requestGeoThenStart(true);
      });
    }
    restoreRadius(radiusSelect);
    if (radiusSelect) {
      radiusSelect.addEventListener('change', function () {
        var n = currentRadiusKm();
        radiusSelect.value = String(n);
        persistRadius(n);
        if (!state.active) return;
        state.retryAfterMs = 0;
        if (state.userLat == null) return;
        if (state.paused) {
          setPaused(false);
        } else {
          startPoll();
        }
      });
    }
    function onCoordEnter(ev) {
      if (ev.key === 'Enter') {
        ev.preventDefault();
        applyManualCoords();
      }
    }
    if (latInput) latInput.addEventListener('keydown', onCoordEnter);
    if (lonInput) lonInput.addEventListener('keydown', onCoordEnter);
    if (radiusSelect) radiusSelect.addEventListener('keydown', onCoordEnter);

    fillApiKeyInput();
    var keyInput = $('aprsApiKeyInput');
    var keySaveBtn = $('aprsApiKeySaveBtn');
    if (keySaveBtn) {
      keySaveBtn.addEventListener('click', saveApiKeyFromInput);
    }
    if (keyInput) {
      keyInput.addEventListener('keydown', function (ev) {
        if (ev.key === 'Enter') {
          ev.preventDefault();
          saveApiKeyFromInput();
        }
      });
    }

    document.querySelectorAll('.tab').forEach(function (tab) {
      tab.addEventListener('click', function () {
        window.setTimeout(function () {
          onTabMaybeChanged();
          updateSerialButtons();
        }, 0);
      });
    });

    document.addEventListener('visibilitychange', function () {
      if (document.visibilityState === 'hidden') {
        clearPoll();
      } else if (state.active && !state.paused && state.userLat != null && !currentSearchValue()) {
        startPoll();
      }
    });

    window.addEventListener('resize', function () {
      if (state.active) {
        drawMap(performance.now());
      }
    });

    updateSerialButtons();
  }

  window.aprsRefreshI18n = function () {
    updateMeta();
    if (state.selectedName) selectStation(state.selectedName, { skipPush: true });
    var pauseBtn = $('aprsPauseBtn');
    if (pauseBtn) {
      pauseBtn.textContent = state.paused ? tr('aprsResume') : tr('aprsPause');
    }
    updateSerialButtons();
  };

  if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', bindUi);
  } else {
    bindUi();
  }
})();
