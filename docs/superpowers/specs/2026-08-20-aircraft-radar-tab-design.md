# Aircraft Radar Tab Design (方案 1)

## Summary

Add a new website tab **「飞机雷达探测」** on the Syrup flasher site (`docs/index.html` / GitHub Pages). The tab uses browser geolocation and the OpenSky Network ADS-B API to show nearby aircraft on a circular scanning radar UI, with selectable aircraft details.

**This phase is web-only.** Handheld radio serial control, airband tune, and on-radio aircraft display are explicitly out of scope (future phases).

## Goals

- Discover aircraft near the user’s current location from the browser
- Present a radar-style scan visualization with distance rings and moving targets
- Show callsign, altitude, ground speed, track, distance, and bearing for a selected aircraft
- Fit existing Syrup site patterns (tabs, i18n, styling) without touching flash / writefreq / serial code

## Non-Goals (this phase)

- Programming cable / Web Serial
- Tuning VFO to airband ATC frequencies
- Pushing aircraft info to the radio LCD
- Manual lat/lon or city override
- Local RTL-SDR / dump1090 feeds
- Backend proxy or server of our own

## Architecture

```
Browser (GitHub Pages)
  ├─ Geolocation API → user lat/lon (radar center)
  ├─ OpenSky REST /api/states/all?bbox=… → state vectors
  └─ radar.js → radar canvas/SVG + detail panel + poll loop
```

Data path is browser → OpenSky (CORS enabled). No firmware or serial involvement.

## UI / Interaction

### Tab integration

- New tab button: **飞机雷达探测** (`data-tab="radar"`), next to existing tabs
- Matching `#radar-content` panel
- Strings in `docs/js/lang.js` (zh + en)
- Leaving the tab pauses polling; returning resumes

### Layout (inside radar tab)

1. **Status bar**: geolocation state, aircraft count, last refresh time, Refresh / Pause
2. **Radar**: circular instrument view (primary visual) — rotating sweep, concentric range rings, aircraft as dots (optional short callsign labels)
3. **Detail panel**: selected aircraft — callsign, ICAO24, altitude, ground speed, track, distance, bearing

### Interaction flow

1. Enter tab → request geolocation
2. On success → fetch OpenSky bbox around user → render targets → start scan animation
3. Click aircraft (radar or list) → highlight + update detail panel
4. Pause → stop polling; scan animation keeps running; Refresh → immediate fetch
5. Geolocation denied → clear error message; do not invent targets

### Visual

- Reuse Syrup site colors/fonts
- Dark circular radar area for instrument feel
- No dashboard clutter, cards, or multi-widget hero layout

### Defaults

| Parameter | Value |
|-----------|--------|
| Search radius | **80 km** (bbox derived from center; UI constant, not user-editable in this phase) |
| Poll interval | **10 s** (matches OpenSky anonymous time resolution) |
| Pause when tab inactive | yes |

## Data Source

- **API**: OpenSky Network `GET /api/states/all` with `lamin`, `lomin`, `lamax`, `lomax`
- **Auth**: anonymous (no credentials in the static site)
- **Fields used**: callsign, icao24, lat, lon, baro/geo altitude, velocity, true_track (ignore incomplete positions)
- **Derived**: distance and bearing from user position for plot and detail panel
- **Rate limits**: respect anonymous quotas; on `429`, show status and retry with backoff

## Error Handling

| Condition | Behavior |
|-----------|----------|
| Geolocation denied / unavailable | Prompt to allow location; no fetch |
| Network / OpenSky error | Status message; retry on next interval or after backoff |
| HTTP 429 | Status + retry-after / exponential backoff |
| Zero aircraft in bbox | Empty radar + “附近暂无 ADS-B 目标” (i18n) |
| Tab hidden / switched away | Pause polling |

## File Touch List

| File | Change |
|------|--------|
| `docs/index.html` | Tab button + `#radar-content` markup |
| `docs/js/radar.js` | New: geolocation, OpenSky client, poll, radar render, selection |
| `docs/css/*` (existing or small addition) | Radar layout / canvas styles |
| `docs/js/lang.js` | zh/en strings for tab and radar UI |
| `docs/js/flash.js` (or tab switcher) | Only if needed to pause/resume on tab change — prefer self-contained listeners in `radar.js` |

Do **not** modify flash, calibration, writefreq, or firmware for this phase.

## Future Phases (not in this spec)

1. Web Serial: tune radio VFO to nearby airport airband frequency when selecting an aircraft
2. Firmware: display callsign / altitude / distance on the radio LCD

## Testing

- Chrome/Edge with location permission granted: aircraft appear near a known busy airspace (or empty state inland)
- Location denied: error copy only
- Switch away from tab: network polling stops
- Pause / Refresh controls behave as specified
- zh/en strings switch with existing language toggle
