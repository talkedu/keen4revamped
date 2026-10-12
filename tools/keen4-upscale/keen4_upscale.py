#!/usr/bin/env python3
"""keen4-upscale: builds the remaster's 4x art set from keen4-extract output.

    python3 keen4_upscale.py EXTRACTED_DIR ART_DIR [--method scale4x|nearest|realesrgan]
                             [--model NAME] [--realesrgan PATH] [--only tile16,tile16m,sprite]

Steps (docs/ART.md):
  1. Tiles are upscaled as whole sheets, 18 tiles wide as the levels lay them
     out, then cut back into 64 px tiles, so edges match and no seams show.
  2. Sprites are upscaled per animation strip (frames side by side), so all
     frames of an animation come out consistent.
  3. Transparency is upscaled on its own and re-thresholded to a hard edge,
     because AI models blur alpha and leave halos.
  4. ART_DIR/manifest.txt lists every image; ART_DIR/review.html shows the
     originals next to the results. Add "bad" to a manifest line to fall back
     to the built-in upscale for that asset until it is redone.

Methods: scale4x (Scale2x twice, built in, no new colours), nearest, or an AI
model through the realesrgan-ncnn-vulkan program (Real-ESRGAN). Check a model's
licence before using its output. Only the Python standard library is needed.
The output is derived from the game data: keep it out of the repository.
"""

import argparse
import os
import shutil
import struct
import subprocess
import sys
import tempfile
import zlib

DENSITY = 4
SHEET_COLS = 18
STRIP_PAD = 2


# --- PNG ---------------------------------------------------------------------

def read_png(path):
    """Returns (width, height, rows) with rows of RGBA tuples (8-bit, non-interlaced)."""
    with open(path, "rb") as f:
        data = f.read()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError(f"{path}: not a PNG")
    pos, idat, palette, trns = 8, b"", None, None
    while pos < len(data):
        n = struct.unpack(">I", data[pos:pos + 4])[0]
        kind = data[pos + 4:pos + 8]
        body = data[pos + 8:pos + 8 + n]
        if kind == b"IHDR":
            w, h, depth, ctype, _, _, interlace = struct.unpack(">IIBBBBB", body)
        elif kind == b"PLTE":
            palette = [tuple(body[i:i + 3]) for i in range(0, len(body), 3)]
        elif kind == b"tRNS":
            trns = body
        elif kind == b"IDAT":
            idat += body
        pos += 12 + n
    if depth != 8 or interlace:
        raise ValueError(f"{path}: only 8-bit non-interlaced PNGs are supported")
    channels = {0: 1, 2: 3, 3: 1, 4: 2, 6: 4}[ctype]
    raw = zlib.decompress(idat)
    stride = w * channels
    rows, prev = [], bytearray(stride)
    for y in range(h):
        f = raw[y * (stride + 1)]
        line = bytearray(raw[y * (stride + 1) + 1:(y + 1) * (stride + 1)])
        for i in range(stride):
            a = line[i - channels] if i >= channels else 0
            b = prev[i]
            c = prev[i - channels] if i >= channels else 0
            if f == 1:
                line[i] = (line[i] + a) & 255
            elif f == 2:
                line[i] = (line[i] + b) & 255
            elif f == 3:
                line[i] = (line[i] + ((a + b) >> 1)) & 255
            elif f == 4:
                p = a + b - c
                pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
                pr = a if pa <= pb and pa <= pc else (b if pb <= pc else c)
                line[i] = (line[i] + pr) & 255
        prev = line
        row = []
        for x in range(w):
            px = line[x * channels:(x + 1) * channels]
            if ctype == 6:
                row.append(tuple(px))
            elif ctype == 2:
                row.append((px[0], px[1], px[2], 255))
            elif ctype == 0:
                row.append((px[0], px[0], px[0], 255))
            elif ctype == 4:
                row.append((px[0], px[0], px[0], px[1]))
            else:
                r, g, bb = palette[px[0]]
                al = trns[px[0]] if trns and px[0] < len(trns) else 255
                row.append((r, g, bb, al))
        rows.append(row)
    return w, h, rows


def write_png(path, w, h, rows):
    raw = bytearray()
    for row in rows:
        raw.append(0)
        for px in row:
            raw.extend(px)

    def chunk(kind, body):
        c = struct.pack(">I", len(body)) + kind + body
        return c + struct.pack(">I", zlib.crc32(kind + body) & 0xFFFFFFFF)

    png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0))
    png += chunk(b"IDAT", zlib.compress(bytes(raw), 6)) + chunk(b"IEND", b"")
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "wb") as f:
        f.write(png)


# --- upscalers ---------------------------------------------------------------

def scale2x(rows):
    h, w = len(rows), len(rows[0])
    out = [[None] * (w * 2) for _ in range(h * 2)]
    for y in range(h):
        up, row, down = rows[max(y - 1, 0)], rows[y], rows[min(y + 1, h - 1)]
        for x in range(w):
            e = row[x]
            b, hh = up[x], down[x]
            d, f = row[max(x - 1, 0)], row[min(x + 1, w - 1)]
            e0 = e1 = e2 = e3 = e
            if b != hh and d != f:
                e0 = d if d == b else e
                e1 = f if b == f else e
                e2 = d if d == hh else e
                e3 = f if hh == f else e
            out[y * 2][x * 2], out[y * 2][x * 2 + 1] = e0, e1
            out[y * 2 + 1][x * 2], out[y * 2 + 1][x * 2 + 1] = e2, e3
    return out


def nearest(rows, factor=DENSITY):
    return [[px for px in row for _ in range(factor)] for row in rows for _ in range(factor)]


class RealEsrgan:
    """Runs realesrgan-ncnn-vulkan on whole images; alpha is handled separately."""

    def __init__(self, program, model):
        self.program, self.model = program, model

    def run(self, rows):
        h, w = len(rows), len(rows[0])
        with tempfile.TemporaryDirectory() as tmp:
            src, dst = os.path.join(tmp, "in.png"), os.path.join(tmp, "out.png")
            write_png(src, w, h, rows)
            subprocess.run([self.program, "-i", src, "-o", dst, "-s", str(DENSITY), "-n", self.model],
                           check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            ow, oh, out = read_png(dst)
        if (ow, oh) != (w * DENSITY, h * DENSITY):
            raise RuntimeError(f"model returned {ow}x{oh}, expected {w * DENSITY}x{h * DENSITY}")
        return out


def upscale(rows, method, ai):
    """4x upscale of RGBA rows. Transparency is kept as a hard edge."""
    if method == "nearest":
        return nearest(rows)
    if method == "scale4x":
        return scale2x(scale2x(rows))
    # AI: colour and mask go through separately; transparent pixels take the
    # colour of an opaque neighbour so the model sees no fake edges.
    h, w = len(rows), len(rows[0])
    colour = [[(px[0], px[1], px[2], 255) for px in row] for row in rows]
    for y in range(h):
        for x in range(w):
            if rows[y][x][3] == 0:
                for dy, dx in ((0, -1), (0, 1), (-1, 0), (1, 0)):
                    yy, xx = y + dy, x + dx
                    if 0 <= yy < h and 0 <= xx < w and rows[yy][xx][3]:
                        colour[y][x] = colour[yy][xx]
                        break
    big = ai.run(colour)
    has_alpha = any(px[3] < 255 for row in rows for px in row)
    if not has_alpha:
        return big
    mask = [[(px[3], px[3], px[3], 255) for px in row] for row in rows]
    bigmask = ai.run(mask)
    return [[(c[0], c[1], c[2], 255 if m[0] >= 128 else 0) for c, m in zip(crow, mrow)]
            for crow, mrow in zip(big, bigmask)]


# --- pipeline ----------------------------------------------------------------

def crop(rows, x, y, w, h):
    return [row[x:x + w] for row in rows[y:y + h]]


def is_empty(rows):
    return all(px[3] == 0 for row in rows for px in row)


def parse_index(path):
    sheets, strips = [], []
    with open(path) as f:
        for line in f:
            parts = line.split()
            if not parts or parts[0].startswith("#"):
                continue
            if parts[0] == "sheet":
                sheets.append((parts[1], parts[2], int(parts[3]), int(parts[4])))
            elif parts[0] == "strip":
                strips.append([int(p) for p in parts[1:]])
    return sheets, strips


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("extracted")
    ap.add_argument("art")
    ap.add_argument("--method", choices=["scale4x", "nearest", "realesrgan"], default="scale4x")
    ap.add_argument("--model", default="realesrgan-x4plus-anime")
    ap.add_argument("--realesrgan", default="realesrgan-ncnn-vulkan", help="path of the Real-ESRGAN program")
    ap.add_argument("--only", default="tile16,tile16m,sprite")
    args = ap.parse_args()

    ai = None
    if args.method == "realesrgan":
        program = shutil.which(args.realesrgan) or (args.realesrgan if os.path.exists(args.realesrgan) else None)
        if not program:
            sys.exit(f"{args.realesrgan} not found; install Real-ESRGAN (ncnn-vulkan) or use --method scale4x")
        ai = RealEsrgan(program, args.model)

    kinds = set(args.only.split(","))
    sheets, strips = parse_index(os.path.join(args.extracted, "index.txt"))
    manifest, review = [], []

    for kind, sheet, cols, count in sheets:
        if kind not in kinds:
            continue
        print(f"{kind}: upscaling {sheet} in context...", flush=True)
        _, _, rows = read_png(os.path.join(args.extracted, sheet))
        big = upscale(rows, args.method, ai)
        t = 16 * DENSITY
        for n in range(count):
            x, y = (n % cols) * t, (n // cols) * t
            original = os.path.join(args.extracted, kind, f"{n:04d}.png")
            if not os.path.exists(original):
                continue
            tile = crop(big, x, y, t, t)
            if kind == "tile16m" and is_empty(tile):
                continue
            name = f"{kind}/{n:04d}.png"
            write_png(os.path.join(args.art, name), t, t, tile)
            manifest.append(f"{kind} {n} {name}")
            review.append((kind, n, os.path.relpath(original, args.art), name))

    if "sprite" in kinds:
        print(f"sprite: upscaling {len(strips)} animation strips...", flush=True)
        for strip in strips:
            frames = [read_png(os.path.join(args.extracted, "sprite", f"{c:04d}.png")) for c in strip]
            fw, fh = frames[0][0], frames[0][1]
            sw = len(frames) * (fw + STRIP_PAD) + STRIP_PAD
            sheet = [[(0, 0, 0, 0)] * sw for _ in range(fh + 2 * STRIP_PAD)]
            for i, (_, _, rows) in enumerate(frames):
                ox = STRIP_PAD + i * (fw + STRIP_PAD)
                for y in range(fh):
                    sheet[STRIP_PAD + y][ox:ox + fw] = rows[y]
            big = upscale(sheet, args.method, ai)
            for i, c in enumerate(strip):
                ox = (STRIP_PAD + i * (fw + STRIP_PAD)) * DENSITY
                img = crop(big, ox, STRIP_PAD * DENSITY, fw * DENSITY, fh * DENSITY)
                name = f"sprite/{c:04d}.png"
                write_png(os.path.join(args.art, name), fw * DENSITY, fh * DENSITY, img)
                manifest.append(f"sprite {c} {name}")
                review.append(("sprite", c, os.path.relpath(os.path.join(args.extracted, "sprite", f"{c:04d}.png"),
                                                            args.art), name))

    os.makedirs(args.art, exist_ok=True)
    with open(os.path.join(args.art, "manifest.txt"), "w") as f:
        f.write(f"# keen4-upscale --method {args.method}" + (f" --model {args.model}" if ai else "") + "\n")
        f.write("# kind number file [anchor ax ay] [bad]  (art at 4x density)\n")
        f.write("\n".join(manifest) + "\n")

    with open(os.path.join(args.art, "review.html"), "w") as f:
        f.write("<!doctype html><meta charset=utf-8><title>keen4-upscale review</title><style>"
                "body{background:#222;color:#ddd;font:12px sans-serif}div{display:inline-block;margin:6px;"
                "text-align:center}img{image-rendering:pixelated;background:#707;margin:2px}</style>\n"
                "<p>Original (scaled 4x) and result. Add <b>bad</b> to the asset's line in manifest.txt "
                "to fall back to the built-in upscale.</p>\n")
        for kind, n, original, name in review:
            f.write(f'<div><img src="{original}" style="zoom:4"><img src="{name}"><br>{kind} {n}</div>\n')

    print(f"wrote {len(manifest)} images, manifest.txt and review.html to {args.art}")


if __name__ == "__main__":
    main()
