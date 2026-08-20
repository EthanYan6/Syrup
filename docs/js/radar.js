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
 * Aircraft radar tab: browser geolocation + OpenSky Network (web-only).
 */
(function () {
  'use strict';

  var RADIUS_KM = 80;
  var POLL_MS = 10000;
  /** Same-origin proxy (docs/serve.py). Direct OpenSky calls fail CORS in browsers. */
  var AIRCRAFT_API = '/api/aircraft';

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
    geoAsked: false
  };

  function $(id) {
    return document.getElementById(id);
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

  function selectAircraft(icao) {
    state.selectedIcao = icao || null;
    var empty = $('radarDetailEmpty');
    var fields = $('radarDetailFields');
    var ac = null;
    var i;
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
    var flight = parseFlightInfo(ac.callsign);
    var flightText = flight.flightId || tr('radarFlightUnknown');
    var airlineText = '—';
    if (flight.airlineCode) {
      airlineText = flight.known
        ? (flight.airlineName + ' (' + flight.airlineCode + ')')
        : (tr('radarAirlineUnknown') + ' (' + flight.airlineCode + ')');
    }
    var map = {
      radarFieldCallsign: ac.callsign || '—',
      radarFieldFlight: flightText,
      radarFieldAirline: airlineText,
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
      var flight = parseFlightInfo(ac.callsign);
      var label = flight.flightId || ac.callsign || ac.icao;
      var metaBits = [formatDist(ac.distanceKm), formatTrack(ac.bearing)];
      if (flight.known && flight.airlineName) {
        metaBits.unshift(flight.airlineName);
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
      if (dist > RADIUS_KM * 1.05) return;
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
      radiusKm: String(RADIUS_KM)
    });

    state.fetching = true;
    setStatus('radarStatusFetching');

    fetch(AIRCRAFT_API + '?' + params.toString(), { cache: 'no-store' })
      .then(function (res) {
        if (res.status === 429) {
          state.retryAfterMs = Date.now() + 30000;
          throw new Error('rate_limited');
        }
        if (res.status === 404) {
          throw new Error('no_proxy');
        }
        if (!res.ok) {
          throw new Error('http_' + res.status);
        }
        state.retryAfterMs = 0;
        return res.json();
      })
      .then(function (data) {
        if (data && data.error) {
          throw new Error(data.error);
        }
        state.aircraft = parseAircraftPayload(data);
        var d = new Date();
        state.lastUpdated =
          String(d.getHours()).padStart(2, '0') + ':' +
          String(d.getMinutes()).padStart(2, '0') + ':' +
          String(d.getSeconds()).padStart(2, '0');
        updateMeta();
        if (state.selectedIcao) {
          selectAircraft(state.selectedIcao);
        } else {
          renderList();
        }
        if (state.aircraft.length === 0) {
          setStatus('radarStatusEmpty');
        } else {
          setStatus('radarStatusOk', { n: state.aircraft.length });
        }
      })
      .catch(function (err) {
        if (err && err.message === 'rate_limited') {
          setStatus('radarStatusRateLimited');
        } else if (err && err.message === 'no_proxy') {
          setStatus('radarStatusNeedProxy');
        } else {
          setStatus('radarStatusError');
          console.warn('radar fetch', err);
        }
      })
      .finally(function () {
        state.fetching = false;
      });
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

  function requestGeoThenStart() {
    if (!navigator.geolocation) {
      setStatus('radarStatusNoGeo');
      return;
    }
    setStatus('radarStatusLocating');
    state.geoAsked = true;
    navigator.geolocation.getCurrentPosition(
      function (pos) {
        state.userLat = pos.coords.latitude;
        state.userLon = pos.coords.longitude;
        setStatus('radarStatusFetching');
        startPoll();
      },
      function () {
        setStatus('radarStatusGeoDenied');
      },
      { enableHighAccuracy: false, timeout: 15000, maximumAge: 60000 }
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
    ctx.fillText(Math.round(RADIUS_KM / 3) + ' km', cx + 4, cy - r * 0.33 + 4);
    ctx.fillText(Math.round((RADIUS_KM * 2) / 3) + ' km', cx + 4, cy - r * 0.66 + 4);
    ctx.fillText(RADIUS_KM + ' km', cx + 4, cy - r + 12);

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
      var frac = Math.min(1, ac.distanceKm / RADIUS_KM);
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
      var label = ac.callsign || ac.icao;
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
      var frac = Math.min(1, ac.distanceKm / RADIUS_KM);
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
    var canvas = $('radarCanvas');
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
    if (canvas) {
      canvas.addEventListener('click', hitTestCanvas);
    }

    document.querySelectorAll('.tab').forEach(function (tab) {
      tab.addEventListener('click', function () {
        // Run after flash.js tab handler updates .active
        window.setTimeout(onTabMaybeChanged, 0);
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
  }

  window.radarRefreshI18n = function () {
    updateMeta();
    if (state.selectedIcao) selectAircraft(state.selectedIcao);
    var pauseBtn = $('radarPauseBtn');
    if (pauseBtn) {
      pauseBtn.textContent = state.paused ? tr('radarResume') : tr('radarPause');
    }
  };

  if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', bindUi);
  } else {
    bindUi();
  }
})();
