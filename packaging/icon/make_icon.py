#!/usr/bin/env python3
"""Draws the keen4revamped icon (an original "K4" badge, no game art) and
writes keen4revamped.png (256x256) and keen4revamped.ico (16, 32, 64, 128, 256 px).

Standard library only. Run from anywhere: python3 packaging/icon/make_icon.py
"""
import os
import struct
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
SIZE = 256
SS = 3  # supersamples per axis

# Shapes in unit coordinates (0..1, y down).
K = [
    [(0.16, 0.18), (0.31, 0.18), (0.31, 0.80), (0.16, 0.80)],
    [(0.27, 0.50), (0.50, 0.18), (0.67, 0.18), (0.39, 0.57)],
    [(0.34, 0.46), (0.67, 0.80), (0.50, 0.80), (0.27, 0.58)],
]
FOUR = [
    [(0.75, 0.38), (0.85, 0.38), (0.85, 0.82), (0.75, 0.82)],
    [(0.75, 0.38), (0.85, 0.38), (0.66, 0.68), (0.56, 0.68)],
    [(0.56, 0.62), (0.91, 0.62), (0.91, 0.70), (0.56, 0.70)],
]
STARS = [(0.78, 0.17, 0.022), (0.88, 0.27, 0.014), (0.62, 0.11, 0.012), (0.12, 0.10, 0.013)]
OUTLINE = 0.022


def in_poly(x, y, poly):
    inside = False
    n = len(poly)
    for i in range(n):
        x1, y1 = poly[i]
        x2, y2 = poly[(i + 1) % n]
        if (y1 > y) != (y2 > y) and x < (x2 - x1) * (y - y1) / (y2 - y1) + x1:
            inside = not inside
    return inside


def in_shapes(x, y, shapes):
    return any(in_poly(x, y, p) for p in shapes)


def near_shapes(x, y, shapes, d):
    for dx, dy in ((d, 0), (-d, 0), (0, d), (0, -d), (d * .7, d * .7), (-d * .7, d * .7),
                   (d * .7, -d * .7), (-d * .7, -d * .7)):
        if in_shapes(x + dx, y + dy, shapes):
            return True
    return False


def in_rounded_rect(x, y, m=0.03, r=0.18):
    lo, hi = m, 1 - m
    if not (lo <= x <= hi and lo <= y <= hi):
        return False
    cx = min(max(x, lo + r), hi - r)
    cy = min(max(y, lo + r), hi - r)
    return (x - cx) ** 2 + (y - cy) ** 2 <= r * r


def lerp(a, b, t):
    return tuple(a[i] + (b[i] - a[i]) * t for i in range(3))


def sample(x, y):
    """Colour (r, g, b, a) of one point."""
    if not in_rounded_rect(x, y):
        return (0, 0, 0, 0)
    if in_shapes(x, y, K):
        top, bot = (255, 226, 64), (255, 160, 20)
        return lerp(top, bot, (y - 0.18) / 0.62) + (255,)
    if in_shapes(x, y, FOUR):
        return lerp((255, 96, 72), (210, 30, 40), (y - 0.38) / 0.44) + (255,)
    if near_shapes(x, y, K + FOUR, OUTLINE):
        return (16, 12, 40, 255)
    if not in_rounded_rect(x, y, m=0.06, r=0.15):
        return (14, 20, 60, 255)  # rim
    # Green hill along the bottom.
    if (x - 0.5) ** 2 + (y - 1.55) ** 2 <= 0.72 ** 2:
        return lerp((70, 190, 70), (30, 120, 40), max(0.0, (y - 0.83) / 0.14)) + (255,)
    for sx, sy, sr in STARS:
        if abs(x - sx) + abs(y - sy) <= sr * 1.6 and (abs(x - sx) < sr * .35 or abs(y - sy) < sr * .35):
            return (255, 255, 230, 255)
    return lerp((24, 34, 110), (70, 120, 220), y) + (255,)


def render():
    px = []
    step = 1.0 / (SIZE * SS)
    for j in range(SIZE):
        row = []
        for i in range(SIZE):
            acc = [0.0, 0.0, 0.0, 0.0]
            for sj in range(SS):
                for si in range(SS):
                    r, g, b, a = sample((i * SS + si + .5) * step, (j * SS + sj + .5) * step)
                    acc[0] += r * a
                    acc[1] += g * a
                    acc[2] += b * a
                    acc[3] += a
            a = acc[3]
            if a:
                row.append((acc[0] / a, acc[1] / a, acc[2] / a, a / (SS * SS)))
            else:
                row.append((0.0, 0.0, 0.0, 0.0))
        px.append(row)
    return px


def downscale(px, size):
    """Box filter from SIZE to size (size divides SIZE)."""
    f = SIZE // size
    out = []
    for j in range(size):
        row = []
        for i in range(size):
            acc = [0.0, 0.0, 0.0, 0.0]
            for y in range(j * f, (j + 1) * f):
                for x in range(i * f, (i + 1) * f):
                    r, g, b, a = px[y][x]
                    acc[0] += r * a
                    acc[1] += g * a
                    acc[2] += b * a
                    acc[3] += a
            a = acc[3]
            row.append((acc[0] / a, acc[1] / a, acc[2] / a, a / (f * f)) if a else (0, 0, 0, 0))
        out.append(row)
    return out


def png(px):
    h, w = len(px), len(px[0])
    raw = bytearray()
    for row in px:
        raw.append(0)
        for r, g, b, a in row:
            raw += bytes((round(r), round(g), round(b), round(a)))

    def chunk(tag, data):
        return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data))

    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0)) +
            chunk(b"IDAT", zlib.compress(bytes(raw), 9)) + chunk(b"IEND", b""))


def ico(images):
    """images: list of (size, png bytes). PNG-compressed ICO entries."""
    head = struct.pack("<HHH", 0, 1, len(images))
    offset = 6 + 16 * len(images)
    entries, blobs = b"", b""
    for size, data in images:
        dim = 0 if size >= 256 else size
        entries += struct.pack("<BBBBHHII", dim, dim, 0, 0, 1, 32, len(data), offset)
        offset += len(data)
        blobs += data
    return head + entries + blobs


def main():
    px = render()
    big = png(px)
    with open(os.path.join(HERE, "keen4revamped.png"), "wb") as f:
        f.write(big)
    images = [(256, big)] + [(s, png(downscale(px, s))) for s in (128, 64, 32, 16)]
    with open(os.path.join(HERE, "keen4revamped.ico"), "wb") as f:
        f.write(ico(sorted(images)))


if __name__ == "__main__":
    main()
