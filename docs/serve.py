#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Local static + ADS-B / APRS proxy for Syrup docs site.

adsb.fi and api.aprs.fi block cross-origin browser calls. This server
serves docs/ and proxies:
  /api/aircraft  — ADS-B (adsb.fi, then adsb.lol, then OpenSky)
  /api/aprs      — nearby via APRS-IS; callsign lookup via aprs.fi (API key)

Usage (from repo root or docs/):
    python docs/serve.py
  # then open http://127.0.0.1:5500/
"""

from __future__ import annotations

import json
import math
import os
import re
import socket
import time
import urllib.error
import urllib.parse
import urllib.request
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

HOST = "127.0.0.1"
PORT = 5500
DOCS_DIR = Path(__file__).resolve().parent
UA = "SyrupRadarLocal/1.0 (+https://ethanyan6.github.io/Syrup/)"
APRS_UA = "SyrupAprsLocal/1.0 (+https://ethanyan6.github.io/Syrup/)"


def km_to_lat_delta(km: float) -> float:
    return km / 111.32


def km_to_lon_delta(km: float, lat: float) -> float:
    cos = math.cos(math.radians(lat))
    if abs(cos) < 0.01:
        cos = 0.01
    return km / (111.32 * cos)


def _aprs_api_key() -> str:
    key = (os.environ.get("APRSFI_APIKEY") or os.environ.get("APRS_FI_APIKEY") or "").strip()
    if key:
        return key
    key_file = DOCS_DIR / ".aprs-apikey"
    if key_file.is_file():
        return key_file.read_text(encoding="utf-8").strip().splitlines()[0].strip()
    return ""


def fetch_url(url: str, timeout: float = 20.0, user_agent: str | None = None) -> tuple[int, bytes, str]:
    req = urllib.request.Request(
        url,
        headers={
            "User-Agent": user_agent or UA,
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


def _to_float(value):
    if value is None or value == "":
        return None
    try:
        return float(value)
    except (TypeError, ValueError):
        return None


def _to_int(value):
    if value is None or value == "":
        return None
    try:
        return int(float(value))
    except (TypeError, ValueError):
        return None


def normalize_aprs(data: dict) -> dict:
    stations = []
    for row in data.get("entries") or []:
        if not row:
            continue
        # Skip AIS; this tab is APRS.
        if str(row.get("type") or "") == "a":
            continue
        lat = _to_float(row.get("lat"))
        lon = _to_float(row.get("lng"))
        if lat is None or lon is None:
            continue
        name = str(row.get("name") or row.get("srccall") or "").strip()
        if not name:
            continue
        stations.append(
            {
                "name": name,
                "srccall": str(row.get("srccall") or "").strip(),
                "type": str(row.get("type") or "l"),
                "lat": lat,
                "lon": lon,
                "comment": str(row.get("comment") or ""),
                "speed_kmh": _to_float(row.get("speed")),
                "course": _to_float(row.get("course")),
                "altitude_m": _to_float(row.get("altitude")),
                "lasttime": _to_int(row.get("lasttime")),
                "symbol": str(row.get("symbol") or ""),
                "path": str(row.get("path") or ""),
                "phg": str(row.get("phg") or ""),
            }
        )
    return {"source": "aprs.fi", "stations": stations}


def _dm_to_deg(deg: str, minutes: str, hemi: str) -> float:
    d = float(deg) + float(minutes) / 60.0
    if hemi in ("S", "W"):
        d = -d
    return d


def _parse_uncompressed_pos(body: str):
    m = re.match(
        r"^(\d{2})(\d{2}\.\d+)([NS])(.)(\d{3})(\d{2}\.\d+)([EW])(.)",
        body or "",
    )
    if not m:
        return None
    return {
        "lat": _dm_to_deg(m.group(1), m.group(2), m.group(3)),
        "lon": _dm_to_deg(m.group(5), m.group(6), m.group(7)),
        "symbol": m.group(4) + m.group(8),
        "rest": body[m.end() :],
    }


def _parse_aprs_info(info: str):
    if not info:
        return None
    t = info[0]
    start = 0
    if t in ("!", "="):
        start = 1
    elif t in ("/", "@"):
        start = 8
    elif t == ";":
        start = 18
    else:
        return None
    if len(info) <= start:
        return None
    return _parse_uncompressed_pos(info[start:])


def _parse_aprs_packet(line: str) -> dict | None:
    if not line or line.startswith("#"):
        return None
    gt = line.find(">")
    col = line.find(":")
    if gt < 1 or col <= gt:
        return None
    src = line[:gt].strip()
    if not src:
        return None
    info = line[col + 1 :]
    pos = _parse_aprs_info(info)
    if not pos:
        return None
    lat, lon = pos["lat"], pos["lon"]
    if not (-90 <= lat <= 90 and -180 <= lon <= 180):
        return None
    comment = pos.get("rest") or ""
    course = speed = None
    cs = re.match(r"^(\d{3})/(\d{3})(.*)$", comment)
    if cs:
        course = float(cs.group(1))
        speed = float(cs.group(2)) * 1.852
        comment = cs.group(3) or ""
    comment = re.sub(r"^[\s>/]*", "", comment)[:80]
    return {
        "name": src,
        "srccall": src,
        "type": "o" if info[:1] == ";" else "l",
        "lat": lat,
        "lon": lon,
        "comment": comment,
        "speed_kmh": speed,
        "course": course,
        "altitude_m": None,
        "lasttime": int(time.time()),
        "symbol": pos.get("symbol") or "",
        "path": line[gt + 1 : col],
        "phg": "",
    }


def fetch_aprs_is_nearby(lat: float, lon: float, radius_km: float) -> dict:
    filt = f"r/{lat:.4f}/{lon:.4f}/{int(round(radius_km))}"
    login = f"user BD1AHN-TS pass -1 vers SyrupAprs 1.0 filter {filt}\r\n"
    sock = socket.create_connection(("rotate.aprs2.net", 14580), timeout=8)
    buf = b""
    try:
        sock.settimeout(1.0)
        sock.sendall(login.encode("ascii"))
        deadline = time.time() + 5.0
        while time.time() < deadline:
            try:
                chunk = sock.recv(4096)
                if not chunk:
                    break
                buf += chunk
            except socket.timeout:
                continue
    finally:
        try:
            sock.close()
        except OSError:
            pass
    seen: dict[str, dict] = {}
    text = buf.decode("utf-8", errors="replace")
    for raw in text.splitlines():
        st = _parse_aprs_packet(raw.strip())
        if st:
            seen[st["name"]] = st
    return {"source": "aprs-is", "stations": list(seen.values())}


def fetch_aprs_fi(name: str, apikey: str = "") -> dict:
    apikey = (apikey or _aprs_api_key()).strip()
    if not apikey:
        raise RuntimeError("missing_apikey")
    params = {
        "what": "loc",
        "apikey": apikey,
        "format": "json",
        "name": name,
    }
    url = "https://api.aprs.fi/api/get?" + urllib.parse.urlencode(params)
    code, body, _ = fetch_url(url, timeout=22.0, user_agent=APRS_UA)
    if code == 429:
        raise RuntimeError("aprsfi_http_429")
    if code != 200:
        raise RuntimeError(f"aprsfi_http_{code}")
    data = json.loads(body.decode("utf-8", errors="replace"))
    if not isinstance(data, dict):
        raise RuntimeError("aprsfi_bad_json")
    if str(data.get("result") or "") != "ok":
        desc = str(data.get("description") or data.get("result") or "fail")
        raise RuntimeError(f"aprsfi_{desc}")
    return normalize_aprs(data)


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
            self.send_header("Access-Control-Allow-Headers", "Content-Type, X-APRS-ApiKey")
            self.end_headers()
            return
        self.send_error(404)

    def do_GET(self):
        parsed = urllib.parse.urlparse(self.path)
        if parsed.path == "/api/aircraft":
            self.handle_aircraft(parsed.query)
            return
        if parsed.path == "/api/aprs":
            self.handle_aprs(parsed.query)
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

    def handle_aprs(self, query: str):
        qs = urllib.parse.parse_qs(query)
        name = (qs.get("name", [""])[0] or "").strip()
        lat_raw = (qs.get("lat", [""])[0] or "").strip()
        lon_raw = (qs.get("lon", [""])[0] or "").strip()
        lat = lon = None
        try:
            if lat_raw:
                lat = float(lat_raw)
            if lon_raw:
                lon = float(lon_raw)
            radius_km = float(qs.get("radiusKm", ["80"])[0])
        except (TypeError, ValueError):
            self.send_json(400, {"error": "invalid_params"})
            return
        if (lat is None) != (lon is None):
            self.send_json(400, {"error": "invalid_params"})
            return
        if lat is not None and not (-90 <= lat <= 90 and -180 <= lon <= 180):
            self.send_json(400, {"error": "invalid_coords"})
            return
        if not name and lat is None:
            self.send_json(400, {"error": "invalid_params"})
            return
        radius_km = max(5.0, min(250.0, radius_km))
        client_key = (
            self.headers.get("X-APRS-ApiKey")
            or self.headers.get("x-aprs-apikey")
            or (qs.get("apikey", [""])[0] or "")
            or (qs.get("key", [""])[0] or "")
            or ""
        ).strip()
        try:
            if name:
                payload = fetch_aprs_fi(name, apikey=client_key)
            else:
                payload = fetch_aprs_is_nearby(lat, lon, radius_km)
            self.send_json(200, payload)
        except RuntimeError as exc:
            msg = str(exc)
            if msg == "missing_apikey":
                self.send_json(503, {"error": "missing_apikey"})
                return
            if msg == "aprsfi_http_429":
                self.send_json(429, {"error": "rate_limited"})
                return
            self.send_json(502, {"error": "upstream_failed", "detail": [msg]})
        except Exception as exc:  # noqa: BLE001
            self.send_json(502, {"error": "upstream_failed", "detail": [str(exc)]})

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
    print("APRS API:  GET /api/aprs?lat=..&lon=..&radiusKm=80  (or &name=CALL)")
    if not _aprs_api_key():
        print("APRS key:  set APRSFI_APIKEY or docs/.aprs-apikey")
    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        print("\nStopped.")
    finally:
        httpd.server_close()


if __name__ == "__main__":
    main()
