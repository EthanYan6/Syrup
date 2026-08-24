#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Local static + ADS-B proxy for Syrup docs site.

adsb.fi (and other ADS-B APIs) block cross-origin browser calls. This
server serves docs/ and proxies /api/aircraft so the radar tab works
on localhost. Upstream preference: adsb.fi, then adsb.lol, then OpenSky.

Usage (from repo root or docs/):
  python docs/serve.py
  # then open http://127.0.0.1:5500/
"""

from __future__ import annotations

import json
import math
import urllib.error
import urllib.parse
import urllib.request
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

HOST = "127.0.0.1"
PORT = 5500
DOCS_DIR = Path(__file__).resolve().parent
UA = "SyrupRadarLocal/1.0 (+https://ethanyan6.github.io/Syrup/)"


def km_to_lat_delta(km: float) -> float:
    return km / 111.32


def km_to_lon_delta(km: float, lat: float) -> float:
    cos = math.cos(math.radians(lat))
    if abs(cos) < 0.01:
        cos = 0.01
    return km / (111.32 * cos)


def fetch_url(url: str, timeout: float = 20.0) -> tuple[int, bytes, str]:
    req = urllib.request.Request(
        url,
        headers={
            "User-Agent": UA,
            "Accept": "application/json",
        },
        method="GET",
    )
    try:
        with urllib.request.urlopen(req, timeout=timeout) as resp:
            ctype = resp.headers.get("Content-Type", "application/json")
            return resp.getcode(), resp.read(), ctype
    except urllib.error.HTTPError as e:
        body = e.read() if hasattr(e, "read") else b""
        return int(e.code), body, "application/json"


def fetch_opensky(lat: float, lon: float, radius_km: float) -> dict:
    d_lat = km_to_lat_delta(radius_km)
    d_lon = km_to_lon_delta(radius_km, lat)
    qs = urllib.parse.urlencode(
        {
            "lamin": f"{lat - d_lat:.6f}",
            "lomin": f"{lon - d_lon:.6f}",
            "lamax": f"{lat + d_lat:.6f}",
            "lomax": f"{lon + d_lon:.6f}",
        }
    )
    url = f"https://opensky-network.org/api/states/all?{qs}"
    code, body, _ = fetch_url(url)
    if code != 200:
        raise RuntimeError(f"opensky_http_{code}")
    data = json.loads(body.decode("utf-8", errors="replace"))
    aircraft = []
    for row in data.get("states") or []:
        if not row or len(row) < 11:
            continue
        lon_v, lat_v = row[5], row[6]
        if lat_v is None or lon_v is None:
            continue
        alt = row[13] if row[13] is not None else row[7]
        aircraft.append(
            {
                "icao": str(row[0] or "").strip(),
                "callsign": str(row[1] or "").strip(),
                "lat": lat_v,
                "lon": lon_v,
                "alt_m": alt if isinstance(alt, (int, float)) else None,
                "velocity_ms": row[9] if isinstance(row[9], (int, float)) else None,
                "track": row[10] if isinstance(row[10], (int, float)) else None,
                "on_ground": bool(row[8]),
            }
        )
    return {"source": "opensky", "aircraft": aircraft}


def normalize_adsb_ac(data: dict, source: str) -> dict:
    aircraft = []
    for ac in data.get("ac") or []:
        if ac.get("lat") is None or ac.get("lon") is None:
            continue
        alt = ac.get("alt_geom")
        if alt is None:
            alt = ac.get("alt_baro")
        # dump1090 alt often in feet
        alt_m = None
        if isinstance(alt, (int, float)) and alt > -1000:
            alt_m = float(alt) * 0.3048
        gs_kt = ac.get("gs")
        vel = None
        if isinstance(gs_kt, (int, float)):
            vel = float(gs_kt) / 1.94384
        aircraft.append(
            {
                "icao": str(ac.get("hex") or "").strip(),
                "callsign": str(ac.get("flight") or "").strip(),
                "lat": ac["lat"],
                "lon": ac["lon"],
                "alt_m": alt_m,
                "velocity_ms": vel,
                "track": ac.get("track") if isinstance(ac.get("track"), (int, float)) else None,
                "on_ground": bool(ac.get("ground")) or alt == "ground",
            }
        )
    return {"source": source, "aircraft": aircraft}


def _radius_nm(radius_km: float) -> int:
    return max(1, min(250, int(round(radius_km / 1.852))))


def fetch_adsbfi(lat: float, lon: float, radius_km: float) -> dict:
    url = (
        f"https://opendata.adsb.fi/api/v3/lat/{lat:.5f}/lon/{lon:.5f}"
        f"/dist/{_radius_nm(radius_km)}"
    )
    code, body, _ = fetch_url(url)
    if code != 200:
        raise RuntimeError(f"adsbfi_http_{code}")
    data = json.loads(body.decode("utf-8", errors="replace"))
    return normalize_adsb_ac(data, "adsb.fi")


def fetch_adsblol(lat: float, lon: float, radius_km: float) -> dict:
    url = (
        f"https://api.adsb.lol/v2/lat/{lat:.5f}/lon/{lon:.5f}"
        f"/dist/{_radius_nm(radius_km)}"
    )
    code, body, _ = fetch_url(url)
    if code != 200:
        raise RuntimeError(f"adsblol_http_{code}")
    data = json.loads(body.decode("utf-8", errors="replace"))
    return normalize_adsb_ac(data, "adsb.lol")


class Handler(SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=str(DOCS_DIR), **kwargs)

    def end_headers(self):
        self.send_header("Cache-Control", "no-store")
        super().end_headers()

    def do_OPTIONS(self):
        if self.path.startswith("/api/"):
            self.send_response(204)
            self.send_header("Access-Control-Allow-Origin", "*")
            self.send_header("Access-Control-Allow-Methods", "GET, OPTIONS")
            self.send_header("Access-Control-Allow-Headers", "Content-Type")
            self.end_headers()
            return
        self.send_error(404)

    def do_GET(self):
        parsed = urllib.parse.urlparse(self.path)
        if parsed.path == "/api/aircraft":
            self.handle_aircraft(parsed.query)
            return
        return super().do_GET()

    def handle_aircraft(self, query: str):
        qs = urllib.parse.parse_qs(query)
        try:
            lat = float(qs.get("lat", [""])[0])
            lon = float(qs.get("lon", [""])[0])
            radius_km = float(qs.get("radiusKm", ["80"])[0])
        except (TypeError, ValueError):
            self.send_json(400, {"error": "invalid_params"})
            return
        if not (-90 <= lat <= 90 and -180 <= lon <= 180):
            self.send_json(400, {"error": "invalid_coords"})
            return
        radius_km = max(5.0, min(250.0, radius_km))

        errors = []
        for fetcher in (fetch_adsbfi, fetch_adsblol, fetch_opensky):
            try:
                payload = fetcher(lat, lon, radius_km)
                self.send_json(200, payload)
                return
            except Exception as exc:  # noqa: BLE001 — surface to client
                errors.append(str(exc))

        self.send_json(502, {"error": "upstream_failed", "detail": errors})

    def send_json(self, code: int, obj: dict):
        raw = json.dumps(obj, ensure_ascii=False).encode("utf-8")
        self.send_response(code)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Content-Length", str(len(raw)))
        self.end_headers()
        self.wfile.write(raw)

    def log_message(self, fmt: str, *args):
        sys_stdout = __import__("sys").stderr
        sys_stdout.write("%s - %s\n" % (self.address_string(), fmt % args))


def main():
    httpd = ThreadingHTTPServer((HOST, PORT), Handler)
    print(f"Serving {DOCS_DIR}")
    print(f"Open http://{HOST}:{PORT}/")
    print("Radar API: GET /api/aircraft?lat=..&lon=..&radiusKm=80")
    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        print("\nStopped.")
    finally:
        httpd.server_close()


if __name__ == "__main__":
    main()
