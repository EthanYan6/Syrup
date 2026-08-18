#!/usr/bin/env python3
"""Generate 48x32 ST7565 boot icon from the docs icon-cup SVG geometry."""

from __future__ import annotations

import math
from pathlib import Path

from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[1]
OUT_C = ROOT / "App" / "bitmap_syrup.c"
OUT_H = ROOT / "App" / "bitmap_syrup.h"

W, H = 48, 32  # height is 2/3 of the original 48px boot icon
HI = 480  # draw in 120 viewBox * 4


def cubic(p0, p1, p2, p3, n=48):
    pts = []
    for i in range(n + 1):
        t = i / n
        u = 1 - t
        x = u**3 * p0[0] + 3 * u**2 * t * p1[0] + 3 * u * t**2 * p2[0] + t**3 * p3[0]
        y = u**3 * p0[1] + 3 * u**2 * t * p1[1] + 3 * u * t**2 * p2[1] + t**3 * p3[1]
        pts.append((x, y))
    return pts


def rotate(x, y, cx, cy, deg):
    rad = math.radians(deg)
    dx, dy = x - cx, y - cy
    return (
        cx + dx * math.cos(rad) - dy * math.sin(rad),
        cy + dx * math.sin(rad) + dy * math.cos(rad),
    )


def scaled(pts, s):
    return [(x * s, y * s) for x, y in pts]


def draw_cup(size: int) -> Image.Image:
    s = size / 120.0
    img = Image.new("L", (size, size), 255)
    draw = ImageDraw.Draw(img)

    def sw(svg_w: float) -> int:
        return max(1, round(svg_w * s))

    # Straw (rotated 12° around 75,34) and tip (around 75,11)
    straw = [(72, 8), (78, 8), (78, 60), (72, 60)]
    straw = [rotate(x, y, 75, 34, 12) for x, y in straw]
    tip = [(73, 6), (77, 6), (77, 16), (73, 16)]
    tip = [rotate(x, y, 75, 11, 12) for x, y in tip]
    draw.polygon(scaled(straw, s), fill=0)
    draw.polygon(scaled(tip, s), fill=0)

    # Handle: M86 48 c12 2 16 18 8 28
    handle = cubic((86, 48), (98, 50), (102, 66), (94, 76), 40)
    draw.line(scaled(handle, s), fill=0, width=sw(4), joint="curve")

    # Cup body outline + white fill
    body_top = [(34, 38), (86, 38)]
    body_br = cubic((80, 96), (79, 104), (72, 110), (64, 110), 24)
    body_bl = cubic((56, 110), (48, 110), (41, 104), (40, 96), 24)
    body = body_top + [(80, 96)] + body_br + [(56, 110)] + body_bl + [(34, 38)]
    draw.polygon(scaled(body, s), fill=255, outline=0, width=sw(3))
    draw.line(scaled(body, s), fill=0, width=sw(3), joint="curve")

    # Drink surface only — keep the bowl hollow so the face stays visible.
    draw.line(scaled([(40, 68), (80, 68)], s), fill=0, width=sw(2))

    # Rim ellipse
    cx, cy, rx, ry = 60 * s, 38 * s, 27 * s, 8 * s
    box = [cx - rx, cy - ry, cx + rx, cy + ry]
    draw.ellipse(box, fill=250, outline=0, width=sw(3))

    # Face enlarged for 48px LCD (SVG original is too fine to survive downsample).
    r = 6.0 * s
    for ex, ey in ((50, 82), (68, 82)):
        x, y = ex * s, ey * s
        draw.ellipse([x - r, y - r, x + r, y + r], fill=0)
    smile = cubic((50, 92), (56, 99), (64, 99), (70, 92), 20)
    draw.line(scaled(smile, s), fill=0, width=sw(5), joint="curve")

    return img


def fit_canvas(hi: Image.Image) -> Image.Image:
    px = hi.load()
    minx, miny, maxx, maxy = hi.width, hi.height, 0, 0
    for y in range(hi.height):
        for x in range(hi.width):
            if px[x, y] < 250:
                minx, miny = min(minx, x), min(miny, y)
                maxx, maxy = max(maxx, x), max(maxy, y)
    pad = 8
    minx, miny = max(0, minx - pad), max(0, miny - pad)
    maxx, maxy = min(hi.width - 1, maxx + pad), min(hi.height - 1, maxy + pad)
    crop = hi.crop((minx, miny, maxx + 1, maxy + 1))
    crop.thumbnail((W, H), Image.Resampling.LANCZOS)
    canvas = Image.new("L", (W, H), 255)
    canvas.paste(crop, ((W - crop.width) // 2, (H - crop.height) // 2))

    px = canvas.load()
    for y in range(H):
        for x in range(W):
            px[x, y] = 0 if px[x, y] < 160 else 255
    return canvas


def to_st7565(img: Image.Image) -> list[int]:
    px = img.load()
    data = []
    for page in range(H // 8):
        for x in range(W):
            b = 0
            for bit in range(8):
                if px[x, page * 8 + bit] < 128:
                    b |= 1 << bit
            data.append(b)
    return data


def ascii_preview(img: Image.Image) -> str:
    px = img.load()
    rows = []
    for y in range(H):
        rows.append("".join("##" if px[x, y] < 128 else "  " for x in range(W)))
    return "\n".join(rows)


def main() -> None:
    hi = draw_cup(HI)
    icon = fit_canvas(hi)
    data = to_st7565(icon)

    header = (
        "/* Copyright 2023 Dual Tachyon\n"
        " * https://github.com/DualTachyon\n"
        " *\n"
        " * Licensed under the Apache License, Version 2.0 (the \"License\");\n"
        " * you may not use this file except in compliance with the License.\n"
        " * You may obtain a copy of the License at\n"
        " *\n"
        " *     http://www.apache.org/licenses/LICENSE-2.0\n"
        " *\n"
        " *     Unless required by applicable law or agreed to in writing, software\n"
        " *     distributed under the License is distributed on an \"AS IS\" BASIS,\n"
        " *     WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.\n"
        " *     See the License for the specific language governing permissions and\n"
        " *     limitations under the License.\n"
        " */\n"
        "\n"
    )
    lines = [
        header.rstrip(),
        f"/* Auto-generated syrup boot icon, {W}x{H} ST7565 column-page format */",
        "#include <stdint.h>",
        '#include "bitmap_syrup.h"',
        "",
        "const uint8_t BITMAP_Syrup[BITMAP_SYRUP_PAGES * BITMAP_SYRUP_WIDTH] = {",
    ]
    for i in range(0, len(data), 12):
        chunk = ", ".join(f"0x{v:02X}" for v in data[i : i + 12])
        lines.append(f"\t{chunk},")
    lines.append("};")
    lines.append("")
    OUT_C.write_text("\n".join(lines) + "\n", encoding="utf-8")

    OUT_H.write_text(
        header
        + "#ifndef BITMAP_SYRUP_H\n"
        + "#define BITMAP_SYRUP_H\n"
        + "\n"
        + "#include <stdint.h>\n"
        + "\n"
        + f"#define BITMAP_SYRUP_WIDTH  {W}\n"
        + f"#define BITMAP_SYRUP_HEIGHT {H}\n"
        + "#define BITMAP_SYRUP_PAGES  (BITMAP_SYRUP_HEIGHT / 8)\n"
        + "\n"
        + "extern const uint8_t BITMAP_Syrup[BITMAP_SYRUP_PAGES * BITMAP_SYRUP_WIDTH];\n"
        + "\n"
        + "#endif\n",
        encoding="utf-8",
    )
    print(ascii_preview(icon))
    print(f"wrote {OUT_C} ({len(data)} bytes)")


if __name__ == "__main__":
    main()
