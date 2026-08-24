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
 * Aircraft radar tab: browser geolocation + adsb.fi (web-only).
 */
(function () {
  'use strict';

  var POLL_MS = 10000;
  /**
   * Aircraft JSON endpoints (tried in order).
   * 1) Local docs/serve.py → /api/aircraft
   * 2) Optional override: window.SYRUP_AIRCRAFT_API
   * 3) Cloudflare Worker (for GitHub Pages; deploy docs/cloudflare-worker)
   */
  var DEFAULT_REMOTE_AIRCRAFT_API = 'https://syrup-radar.ethanyan6.workers.dev/api/aircraft';

  function aircraftEndpoints() {
    var list = ['/api/aircraft'];
    if (typeof window.SYRUP_AIRCRAFT_API === 'string' && window.SYRUP_AIRCRAFT_API) {
      list.push(window.SYRUP_AIRCRAFT_API);
    }
    list.push(DEFAULT_REMOTE_AIRCRAFT_API);
    return list;
  }

  var state = {
    active: false,
    paused: false,
    userLat: null,
    userLon: null,
    aircraft: [],
    selectedIcao: null,
    pollTimer: null,
    animFrame: null,
    sweepAngle: 0,
    lastAnimTs: 0,
    fetching: false,
    retryAfterMs: 0,
    geoAsked: false,
    manualOverride: false
  };

  function $(id) {
    return document.getElementById(id);
  }

  var RADIUS_STORAGE_KEY = 'syrup-radar-radius-km';

  function currentRadiusKm() {
    var el = $('radarRadiusInput');
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

  function isRadarTabActive() {
    var tab = document.querySelector('.tab.active');
    return !!(tab && tab.dataset.tab === 'radar');
  }

  function toRad(deg) {
    return (deg * Math.PI) / 180;
  }

  function toDeg(rad) {
    return (rad * 180) / Math.PI;
  }

  /** Haversine distance in km */
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

  /** Initial bearing in degrees [0, 360) */
  function bearingDeg(lat1, lon1, lat2, lon2) {
    var φ1 = toRad(lat1);
    var φ2 = toRad(lat2);
    var Δλ = toRad(lon2 - lon1);
    var y = Math.sin(Δλ) * Math.cos(φ2);
    var x = Math.cos(φ1) * Math.sin(φ2) - Math.sin(φ1) * Math.cos(φ2) * Math.cos(Δλ);
    return (toDeg(Math.atan2(y, x)) + 360) % 360;
  }

  function setStatus(key, params) {
    var el = $('radarStatus');
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
    var countEl = $('radarCount');
    var updatedEl = $('radarUpdated');
    if (countEl) {
      countEl.textContent = tr('radarCount', { n: state.aircraft.length });
      countEl.removeAttribute('data-i18n');
    }
    if (updatedEl) {
      if (state.lastUpdated) {
        updatedEl.textContent = tr('radarUpdated', { time: state.lastUpdated });
      } else {
        updatedEl.textContent = tr('radarUpdatedNever');
      }
      updatedEl.removeAttribute('data-i18n');
    }
  }

  function formatAlt(m) {
    if (m == null || !isFinite(m)) return '—';
    var ft = Math.round(m * 3.28084);
    return Math.round(m) + ' m / ' + ft + ' ft';
  }

  function formatSpeed(ms) {
    if (ms == null || !isFinite(ms)) return '—';
    var kmh = Math.round(ms * 3.6);
    var kt = Math.round(ms * 1.94384);
    return kmh + ' km/h / ' + kt + ' kt';
  }

  function formatTrack(deg) {
    if (deg == null || !isFinite(deg)) return '—';
    return Math.round(deg) + '°';
  }

  function formatDist(km) {
    if (km == null || !isFinite(km)) return '—';
    if (km < 10) return km.toFixed(1) + ' km';
    return Math.round(km) + ' km';
  }

  function parseFlightInfo(callsign) {
    if (window.SyrupAirline && typeof window.SyrupAirline.parseCallsign === 'function') {
      return window.SyrupAirline.parseCallsign(callsign);
    }
    return {
      callsign: String(callsign || '').trim(),
      airlineCode: '',
      flightNumber: '',
      flightId: '',
      airlineName: '',
      known: false
    };
  }

  function aircraftIcao(ac) {
    return String((ac && ac.icao) || '').trim().toUpperCase() || '—';
  }

  /** When callsign/flight/airline cannot be parsed, show Mode-S ICAO. */
  function flightDisplay(ac) {
    var flight = parseFlightInfo(ac && ac.callsign);
    var icao = aircraftIcao(ac);
    var parsed = !!(flight && flight.flightId);
    var airlineText = icao;
    if (parsed) {
      if (flight.known && flight.airlineName && flight.airlineName !== flight.airlineCode) {
        airlineText = flight.airlineName + ' (' + flight.airlineCode + ')';
      } else {
        airlineText = flight.airlineCode || icao;
      }
    }
    return {
      flight: flight,
      label: parsed ? flight.flightId : icao,
      callsign: parsed ? (flight.callsign || ac.callsign || icao) : icao,
      flightText: parsed ? flight.flightId : icao,
      airlineText: airlineText
    };
  }

  function selectAircraft(icao, opts) {
    state.selectedIcao = icao || null;
    var empty = $('radarDetailEmpty');
    var fields = $('radarDetailFields');
    var ac = null;
    var i;
    var pushToRadio = !(opts && opts.skipPush);
    for (i = 0; i < state.aircraft.length; i++) {
      if (state.aircraft[i].icao === icao) {
        ac = state.aircraft[i];
        break;
      }
    }
    if (!ac) {
      if (empty) empty.hidden = false;
      if (fields) fields.hidden = true;
      renderList();
      return;
    }
    if (empty) empty.hidden = true;
    if (fields) fields.hidden = false;
    var shown = flightDisplay(ac);
    var flight = shown.flight;
    var map = {
      radarFieldCallsign: shown.callsign,
      radarFieldFlight: shown.flightText,
      radarFieldAirline: shown.airlineText,
      radarFieldIcao: ac.icao || '—',
      radarFieldAlt: formatAlt(ac.alt),
      radarFieldSpeed: formatSpeed(ac.velocity),
      radarFieldTrack: formatTrack(ac.track),
      radarFieldDist: formatDist(ac.distanceKm),
      radarFieldBrg: formatTrack(ac.bearing)
    };
    Object.keys(map).forEach(function (id) {
      var el = $(id);
      if (el) el.textContent = map[id];
    });
    renderList();
    if (pushToRadio) {
      pushSelectedToRadio(ac, flight);
    }
    updateSerialButtons();
  }

  function updateSerialButtons() {
    var api = window.SyrupSerial;
    var connected = !!(api && api.isConnected && api.isConnected());
    var connectBtn = $('radarConnectBtn');
    var pushBtn = $('radarPushBtn');
    if (connectBtn) {
      connectBtn.textContent = connected
        ? tr('radarDisconnect')
        : tr('radarConnect');
      connectBtn.setAttribute('data-i18n', connected ? 'radarDisconnect' : 'radarConnect');
    }
    if (pushBtn) {
      pushBtn.disabled = !connected || !state.selectedIcao;
    }
  }

  function pushSelectedToRadio(ac, flight) {
    var api = window.SyrupSerial;
    if (!api || typeof api.pushAircraft !== 'function') {
      return;
    }
    if (!api.isConnected || !api.isConnected()) {
      setStatus('radarStatusNeedSerial');
      updateSerialButtons();
      return;
    }
    var label = (flight && flight.flightId) || (ac && ac.icao) || '';
    var altM = (ac && typeof ac.alt === 'number' && isFinite(ac.alt)) ? Math.round(ac.alt) : -2147483648;
    var distM = (ac && typeof ac.distanceKm === 'number' && isFinite(ac.distanceKm))
      ? Math.min(65534, Math.round(ac.distanceKm * 1000))
      : 0xFFFF;
    api.pushAircraft({
      callsign: label,
      altitudeM: altM,
      distanceM: distM,
      frequency: 0,
      listen: true,
      openPage: true
    }).then(function () {
      setStatus('radarStatusPushed', { call: String(label || '').trim() || '—' });
      updateSerialButtons();
    }).catch(function (err) {
      console.warn('radar push', err);
      setStatus('radarStatusPushFail');
      updateSerialButtons();
    });
  }

  function renderList() {
    var list = $('radarList');
    if (!list) return;
    list.innerHTML = '';
    var sorted = state.aircraft.slice().sort(function (a, b) {
      return a.distanceKm - b.distanceKm;
    });
    sorted.forEach(function (ac) {
      var li = document.createElement('li');
      li.className = 'radar-list-item' + (ac.icao === state.selectedIcao ? ' is-selected' : '');
      li.setAttribute('role', 'button');
      li.tabIndex = 0;
      li.dataset.icao = ac.icao;
      var shown = flightDisplay(ac);
      var label = shown.label;
      var metaBits = [formatDist(ac.distanceKm), formatTrack(ac.bearing)];
      if (shown.flight.known && shown.flight.airlineName && shown.flight.airlineName !== shown.flight.airlineCode) {
        metaBits.unshift(shown.flight.airlineName);
      }
      li.innerHTML =
        '<span class="radar-list-call">' + escapeHtml(label) + '</span>' +
        '<span class="radar-list-meta">' + escapeHtml(metaBits.join(' · ')) + '</span>';
      li.addEventListener('click', function () {
        selectAircraft(ac.icao);
      });
      li.addEventListener('keydown', function (ev) {
        if (ev.key === 'Enter' || ev.key === ' ') {
          ev.preventDefault();
          selectAircraft(ac.icao);
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

  function parseAircraftPayload(payload) {
    var out = [];
    if (!payload || !payload.aircraft || !Array.isArray(payload.aircraft)) {
      return out;
    }
    if (state.userLat == null || state.userLon == null) {
      return out;
    }
    payload.aircraft.forEach(function (row) {
      if (!row) return;
      var lat = row.lat;
      var lon = row.lon;
      if (lat == null || lon == null || !isFinite(lat) || !isFinite(lon)) return;
      var icao = String(row.icao || '').trim();
      if (!icao) return;
      var callsign = String(row.callsign || '').trim();
      var alt = row.alt_m;
      var velocity = row.velocity_ms;
      var track = row.track;
      var dist = distanceKm(state.userLat, state.userLon, lat, lon);
      if (dist > currentRadiusKm() * 1.05) return;
      out.push({
        icao: icao,
        callsign: callsign,
        lat: lat,
        lon: lon,
        alt: typeof alt === 'number' ? alt : null,
        velocity: typeof velocity === 'number' ? velocity : null,
        track: typeof track === 'number' ? track : null,
        distanceKm: dist,
        bearing: bearingDeg(state.userLat, state.userLon, lat, lon),
        onGround: !!row.on_ground
      });
    });
    return out;
  }

  function fetchAircraft() {
    if (!state.active || state.paused || state.fetching) return;
    if (state.userLat == null || state.userLon == null) return;
    if (document.visibilityState === 'hidden') return;

    var now = Date.now();
    if (state.retryAfterMs && now < state.retryAfterMs) {
      setStatus('radarStatusRateLimited');
      return;
    }

    var params = new URLSearchParams({
      lat: String(state.userLat),
      lon: String(state.userLon),
      radiusKm: String(currentRadiusKm())
    });
    var endpoints = aircraftEndpoints();
    var qi = params.toString();

    state.fetching = true;
    setStatus('radarStatusFetching');

    function tryAt(index) {
      if (index >= endpoints.length) {
        setStatus('radarStatusError');
        state.fetching = false;
        return;
      }
      var base = endpoints[index];
      fetch(base + (base.indexOf('?') >= 0 ? '&' : '?') + qi, { cache: 'no-store' })
        .then(function (res) {
          if (res.status === 429) {
            state.retryAfterMs = Date.now() + 30000;
            throw new Error('rate_limited');
          }
          /* Local Pages has no /api → 404; try next endpoint */
          if (res.status === 404 || res.status === 502 || res.status === 503) {
            throw new Error('try_next');
          }
          if (!res.ok) {
            throw new Error('http_' + res.status);
          }
          return res.json().then(function (data) {
            if (data && data.error) {
              throw new Error('try_next');
            }
            return data;
          });
        })
        .then(function (data) {
          state.retryAfterMs = 0;
          state.aircraft = parseAircraftPayload(data);
          var d = new Date();
          state.lastUpdated =
            String(d.getHours()).padStart(2, '0') + ':' +
            String(d.getMinutes()).padStart(2, '0') + ':' +
            String(d.getSeconds()).padStart(2, '0');
          updateMeta();
          if (state.selectedIcao) {
            selectAircraft(state.selectedIcao, { skipPush: true });
          } else {
            renderList();
          }
          if (state.aircraft.length === 0) {
            setStatus('radarStatusEmpty');
          } else {
            setStatus('radarStatusOk', { n: state.aircraft.length });
          }
          state.fetching = false;
        })
        .catch(function (err) {
          if (err && err.message === 'rate_limited') {
            setStatus('radarStatusRateLimited');
            state.fetching = false;
            return;
          }
          if (err && (err.message === 'try_next' || err.message === 'Failed to fetch' ||
              err.name === 'TypeError')) {
            tryAt(index + 1);
            return;
          }
          console.warn('radar fetch', base, err);
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
    fetchAircraft();
    state.pollTimer = setInterval(fetchAircraft, POLL_MS);
  }

  function fillCoordInputs() {
    var latEl = $('radarLatInput');
    var lonEl = $('radarLonInput');
    if (latEl && state.userLat != null && isFinite(state.userLat)) {
      latEl.value = Number(state.userLat).toFixed(4);
    }
    if (lonEl && state.userLon != null && isFinite(state.userLon)) {
      lonEl.value = Number(state.userLon).toFixed(4);
    }
  }

  function applyManualCoords() {
    var latEl = $('radarLatInput');
    var lonEl = $('radarLonInput');
    var lat = parseFloat(latEl && latEl.value);
    var lon = parseFloat(lonEl && lonEl.value);
    if (!isFinite(lat) || !isFinite(lon) || lat < -90 || lat > 90 || lon < -180 || lon > 180) {
      setStatus('radarStatusBadCoords');
      return;
    }
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
      setStatus('radarStatusNoGeo');
      return;
    }
    setStatus('radarStatusLocating');
    state.geoAsked = true;
    navigator.geolocation.getCurrentPosition(
      function (pos) {
        if (state.manualOverride && !force) return;
        if (force) state.manualOverride = false;
        state.userLat = pos.coords.latitude;
        state.userLon = pos.coords.longitude;
        state.retryAfterMs = 0;
        fillCoordInputs();
        setStatus('radarStatusFetching');
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
        setStatus('radarStatusGeoDenied');
      },
      { enableHighAccuracy: false, timeout: 15000, maximumAge: force ? 0 : 60000 }
    );
  }

  function drawRadar(ts) {
    var canvas = $('radarCanvas');
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

    // Face
    var grad = ctx.createRadialGradient(cx, cy, r * 0.1, cx, cy, r);
    if (dark) {
      grad.addColorStop(0, '#1a2a18');
      grad.addColorStop(1, '#0c140c');
    } else {
      grad.addColorStop(0, '#1f3a22');
      grad.addColorStop(1, '#0e1a10');
    }
    ctx.beginPath();
    ctx.arc(cx, cy, r, 0, Math.PI * 2);
    ctx.fillStyle = grad;
    ctx.fill();
    ctx.strokeStyle = dark ? '#5bb06a' : '#6bcf7a';
    ctx.lineWidth = 2;
    ctx.stroke();

    // Range rings
    ctx.strokeStyle = 'rgba(120, 220, 140, 0.28)';
    ctx.lineWidth = 1;
    [0.33, 0.66, 1].forEach(function (f) {
      ctx.beginPath();
      ctx.arc(cx, cy, r * f, 0, Math.PI * 2);
      ctx.stroke();
    });

    // Cross
    ctx.beginPath();
    ctx.moveTo(cx - r, cy);
    ctx.lineTo(cx + r, cy);
    ctx.moveTo(cx, cy - r);
    ctx.lineTo(cx, cy + r);
    ctx.stroke();

    // Range labels
    ctx.fillStyle = 'rgba(180, 240, 190, 0.55)';
    ctx.font = '11px Nunito, sans-serif';
    ctx.textAlign = 'left';
    ctx.fillText(Math.round(currentRadiusKm() / 3) + ' km', cx + 4, cy - r * 0.33 + 4);
    ctx.fillText(Math.round((currentRadiusKm() * 2) / 3) + ' km', cx + 4, cy - r * 0.66 + 4);
    ctx.fillText(currentRadiusKm() + ' km', cx + 4, cy - r + 12);

    // Sweep wedge
    var ang = toRad(state.sweepAngle - 90);
    ctx.save();
    ctx.beginPath();
    ctx.moveTo(cx, cy);
    ctx.arc(cx, cy, r, ang - 0.35, ang, false);
    ctx.closePath();
    var sweepGrad = ctx.createRadialGradient(cx, cy, 0, cx, cy, r);
    sweepGrad.addColorStop(0, 'rgba(120, 255, 140, 0.35)');
    sweepGrad.addColorStop(1, 'rgba(120, 255, 140, 0.02)');
    ctx.fillStyle = sweepGrad;
    ctx.fill();
    ctx.restore();

    // Sweep line
    ctx.beginPath();
    ctx.moveTo(cx, cy);
    ctx.lineTo(cx + Math.cos(ang) * r, cy + Math.sin(ang) * r);
    ctx.strokeStyle = 'rgba(160, 255, 170, 0.9)';
    ctx.lineWidth = 2;
    ctx.stroke();

    // Center (you)
    ctx.beginPath();
    ctx.arc(cx, cy, 4, 0, Math.PI * 2);
    ctx.fillStyle = '#ffd54a';
    ctx.fill();

    // Aircraft
    state.aircraft.forEach(function (ac) {
      var frac = Math.min(1, ac.distanceKm / currentRadiusKm());
      var brg = toRad(ac.bearing - 90);
      var px = cx + Math.cos(brg) * r * frac;
      var py = cy + Math.sin(brg) * r * frac;
      var selected = ac.icao === state.selectedIcao;
      ctx.beginPath();
      ctx.arc(px, py, selected ? 5 : 3.5, 0, Math.PI * 2);
      ctx.fillStyle = selected ? '#ffd54a' : '#7dff9a';
      ctx.fill();
      if (selected) {
        ctx.strokeStyle = 'rgba(255, 213, 74, 0.8)';
        ctx.lineWidth = 2;
        ctx.stroke();
      }
      var label = flightDisplay(ac).label;
      if (label && (selected || state.aircraft.length < 25)) {
        ctx.fillStyle = 'rgba(220, 255, 220, 0.85)';
        ctx.font = '10px Nunito, sans-serif';
        ctx.textAlign = 'left';
        ctx.fillText(label.slice(0, 8), px + 6, py + 3);
      }
    });
  }

  function animLoop(ts) {
    drawRadar(ts || performance.now());
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
    var btn = $('radarPauseBtn');
    if (btn) {
      btn.textContent = state.paused ? tr('radarResume') : tr('radarPause');
      btn.setAttribute('data-i18n', state.paused ? 'radarResume' : 'radarPause');
    }
    if (state.paused) {
      clearPoll();
      setStatus('radarStatusPaused');
    } else if (state.active && state.userLat != null) {
      startPoll();
    }
  }

  function activate() {
    state.active = true;
    startAnim();
    fillCoordInputs();
    if (state.userLat == null) {
      requestGeoThenStart();
    } else if (!state.paused) {
      startPoll();
    } else {
      setStatus('radarStatusPaused');
    }
  }

  function deactivate() {
    state.active = false;
    clearPoll();
    // Keep animation stopped when leaving to save CPU; restart on re-entry
    stopAnim();
  }

  function onTabMaybeChanged() {
    if (isRadarTabActive()) {
      activate();
    } else {
      deactivate();
    }
  }

  function hitTestCanvas(ev) {
    var canvas = $('radarCanvas');
    if (!canvas || !state.aircraft.length) return;
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
    state.aircraft.forEach(function (ac) {
      var frac = Math.min(1, ac.distanceKm / currentRadiusKm());
      var brg = toRad(ac.bearing - 90);
      var px = cx + Math.cos(brg) * r * frac;
      var py = cy + Math.sin(brg) * r * frac;
      var dx = px - x;
      var dy = py - y;
      var d = Math.sqrt(dx * dx + dy * dy);
      if (d < bestD) {
        bestD = d;
        best = ac;
      }
    });
    if (best) selectAircraft(best.icao);
  }

  function bindUi() {
    var refreshBtn = $('radarRefreshBtn');
    var pauseBtn = $('radarPauseBtn');
    var connectBtn = $('radarConnectBtn');
    var pushBtn = $('radarPushBtn');
    var canvas = $('radarCanvas');
    var queryBtn = $('radarQueryBtn');
    var myLocBtn = $('radarUseMyLocationBtn');
    var radiusSelect = $('radarRadiusInput');
    var latInput = $('radarLatInput');
    var lonInput = $('radarLonInput');
    if (refreshBtn) {
      refreshBtn.addEventListener('click', function () {
        if (!state.active) return;
        if (state.userLat == null) {
          requestGeoThenStart();
          return;
        }
        state.retryAfterMs = 0;
        fetchAircraft();
      });
    }
    if (pauseBtn) {
      pauseBtn.addEventListener('click', function () {
        if (!state.active) return;
        setPaused(!state.paused);
      });
    }
    if (connectBtn) {
      connectBtn.addEventListener('click', function () {
        var api = window.SyrupSerial;
        if (!api) return;
        if (api.isConnected && api.isConnected()) {
          Promise.resolve(api.disconnect && api.disconnect()).finally(function () {
            updateSerialButtons();
            setStatus('radarStatusIdle');
          });
          return;
        }
        Promise.resolve(api.connect && api.connect())
          .then(function () {
            setStatus('radarStatusConnected');
            updateSerialButtons();
          })
          .catch(function (err) {
            console.warn('radar connect', err);
            setStatus('radarStatusNeedSerial');
            updateSerialButtons();
          });
      });
    }
    if (pushBtn) {
      pushBtn.addEventListener('click', function () {
        if (!state.selectedIcao) return;
        var ac = null;
        for (var i = 0; i < state.aircraft.length; i++) {
          if (state.aircraft[i].icao === state.selectedIcao) {
            ac = state.aircraft[i];
            break;
          }
        }
        if (!ac) return;
        pushSelectedToRadio(ac, parseFlightInfo(ac.callsign));
      });
    }
    if (canvas) {
      canvas.addEventListener('click', hitTestCanvas);
    }
    if (queryBtn) {
      queryBtn.addEventListener('click', applyManualCoords);
    }
    if (myLocBtn) {
      myLocBtn.addEventListener('click', function () {
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

    document.querySelectorAll('.tab').forEach(function (tab) {
      tab.addEventListener('click', function () {
        // Run after flash.js tab handler updates .active
        window.setTimeout(function () {
          onTabMaybeChanged();
          updateSerialButtons();
        }, 0);
      });
    });

    document.addEventListener('visibilitychange', function () {
      if (document.visibilityState === 'hidden') {
        clearPoll();
      } else if (state.active && !state.paused && state.userLat != null) {
        startPoll();
      }
    });

    window.addEventListener('resize', function () {
      if (state.active) {
        drawRadar(performance.now());
      }
    });

    updateSerialButtons();
  }

  window.radarRefreshI18n = function () {
    updateMeta();
    if (state.selectedIcao) selectAircraft(state.selectedIcao, { skipPush: true });
    var pauseBtn = $('radarPauseBtn');
    if (pauseBtn) {
      pauseBtn.textContent = state.paused ? tr('radarResume') : tr('radarPause');
    }
    updateSerialButtons();
  };

  if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', bindUi);
  } else {
    bindUi();
  }
})();
