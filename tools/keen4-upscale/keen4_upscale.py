#!/usr/bin/env python3
"""keen4-upscale: builds the remaster's 4x art set from keen4-extract output.

    python3 keen4_upscale.py EXTRACTED_DIR ART_DIR [--method scale4x|nearest|realesrgan]
                             [--model NAME] [--realesrgan PATH] [--only tile16,tile16m,sprite,sprite2]

Steps (docs/ART.md):
  1. Tiles are upscaled as whole sheets, 18 tiles wide as the levels lay them
     out, then cut back into 64 px tiles, so edges match and no seams show.
  2. Sprites are upscaled per animation strip (frames side by side), so all
     frames of an animation come out consistent. Tom's sprites (sprite2, co-op)
     are Keen's recoloured by keen4-extract and upscaled the same way.
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
    """Runs realesrgan-ncnn-vulkan once over a folder of images (loading the
    model once). A Windows build of the program also works from WSL."""

    def __init__(self, program, model, workdir):
        self.program, self.model, self.workdir = program, model, workdir
        self.windows = program.lower().endswith(".exe") and shutil.which("wslpath") is not None

    def path(self, p):
        if not self.windows:
            return p
        return subprocess.run(["wslpath", "-w", p], check=True, capture_output=True, text=True).stdout.strip()

    def run_all(self, images):
        """images: list of RGBA rows; returns their 4x versions in the same order.
        Images of one size go through the program together: mixing sizes in
        one run gave corrupted results for some images."""
        by_size = {}
        for i, rows in enumerate(images):
            by_size.setdefault((len(rows[0]), len(rows)), []).append(i)
        out = [None] * len(images)
        src, dst = os.path.join(self.workdir, "in"), os.path.join(self.workdir, "out")
        for (w, h), indices in by_size.items():
            for d in (src, dst):
                shutil.rmtree(d, ignore_errors=True)
                os.makedirs(d)
            for i in indices:
                write_png(os.path.join(src, f"{i:05d}.png"), w, h, images[i])
            subprocess.run([self.program, "-i", self.path(src), "-o", self.path(dst), "-s", str(DENSITY),
                            "-n", self.model, "-f", "png"], check=True, stdout=subprocess.DEVNULL,
                           stderr=subprocess.DEVNULL)
            for i in indices:
                ow, oh, big = read_png(os.path.join(dst, f"{i:05d}.png"))
                if (ow, oh) != (w * DENSITY, h * DENSITY):
                    raise RuntimeError(f"model returned {ow}x{oh} for a {w}x{h} image")
                out[i] = big
        shutil.rmtree(self.workdir, ignore_errors=True)
        return out


class Upscaler:
    """4x upscale of RGBA images; transparency is kept as a hard edge. With an
    AI model, work is queued and run in one batch by flush()."""

    def __init__(self, method, ai):
        self.method, self.ai, self.jobs = method, ai, []

    def submit(self, rows, done, model_mask=True):
        """model_mask=False keeps the original outline (Scale4x of the mask)
        instead of the model's, for tiles that must line up with neighbours."""
        if self.method == "nearest":
            done(nearest(rows))
        elif self.method == "scale4x":
            done(scale2x(scale2x(rows)))
        else:
            self.jobs.append((rows, done, model_mask))

    def flush(self):
        """Runs the queued jobs, one image size at a time to keep memory low."""
        if not self.jobs:
            return  # scale4x and nearest are done at once
        groups = {}
        for job in self.jobs:
            rows = job[0]
            groups.setdefault((len(rows[0]), len(rows)), []).append(job)
        self.jobs = []
        total = sum(len(g) for g in groups.values())
        print(f"running {self.ai.model} on {total} images in {len(groups)} sizes...", flush=True)
        for jobs in groups.values():
            self._run(jobs)

    def _run(self, jobs):
        # Colour and mask go through the model separately; transparent pixels
        # take the colour of an opaque neighbour so the model sees no fake edges.
        images, plan = [], []
        for rows, done, model_mask in jobs:
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
            colour_index = len(images)
            images.append(colour)
            mask_index = None
            if any(px[3] < 255 for row in rows for px in row):
                if model_mask:
                    mask_index = len(images)
                    images.append([[(px[3], px[3], px[3], 255) for px in row] for row in rows])
                else:
                    mask_index = scale2x(scale2x([[px[3] for px in row] for row in rows]))
            plan.append((colour_index, mask_index, done))
        results = self.ai.run_all(images)
        for colour_index, mask_index, done in plan:
            big = results[colour_index]
            if isinstance(mask_index, list):
                big = [[(c[0], c[1], c[2], m) for c, m in zip(crow, mrow)] for crow, mrow in zip(big, mask_index)]
            elif mask_index is not None:
                bigmask = results[mask_index]
                big = [[(c[0], c[1], c[2], 255 if m[0] >= 128 else 0) for c, m in zip(crow, mrow)]
                       for crow, mrow in zip(big, bigmask)]
            done(big)


# --- pipeline ----------------------------------------------------------------

def crop(rows, x, y, w, h):
    return [row[x:x + w] for row in rows[y:y + h]]


def is_empty(rows):
    return all(px[3] == 0 for row in rows for px in row)


def parse_index(path):
    """Returns the sheets and {"sprite": strips, "sprite2": strips}."""
    sheets, strips = [], {"sprite": [], "sprite2": []}
    with open(path) as f:
        for line in f:
            parts = line.split()
            if not parts or parts[0].startswith("#"):
                continue
            if parts[0] == "sheet":
                sheets.append((parts[1], parts[2], int(parts[3]), int(parts[4])))
            elif parts[0] == "strip":
                strips["sprite"].append([int(p) for p in parts[1:]])
            elif parts[0] == "strip2":
                strips["sprite2"].append([int(p) for p in parts[1:]])
    return sheets, strips


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("extracted")
    ap.add_argument("art")
    ap.add_argument("--method", choices=["scale4x", "nearest", "realesrgan"], default="scale4x")
    ap.add_argument("--model", default="realesrgan-x4plus-anime")
    ap.add_argument("--realesrgan", default="realesrgan-ncnn-vulkan", help="path of the Real-ESRGAN program")
    ap.add_argument("--only", default="tile16,tile16m,sprite,sprite2")
    args = ap.parse_args()

    ai = None
    if args.method != "realesrgan":
        up = Upscaler(args.method, None)
    if args.method == "realesrgan":
        program = shutil.which(args.realesrgan) or (args.realesrgan if os.path.exists(args.realesrgan) else None)
        if not program:
            sys.exit(f"{args.realesrgan} not found; install Real-ESRGAN (ncnn-vulkan) or use --method scale4x")
        ai = RealEsrgan(program, args.model, os.path.join(args.art, ".work"))
    up = Upscaler(args.method, ai)

    kinds = set(args.only.split(","))
    sheets, strips = parse_index(os.path.join(args.extracted, "index.txt"))
    manifest, review = {}, {}

    for kind, sheet, cols, count in sheets:
        if kind not in kinds:
            continue
        print(f"{kind}: {sheet} is upscaled in context", flush=True)
        _, _, rows = read_png(os.path.join(args.extracted, sheet))

        def cut(big, kind=kind, cols=cols, count=count):
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
                manifest[(kind, n)] = f"{kind} {n} {name}"
                review[(kind, n)] = (kind, n, os.path.relpath(original, args.art), name)

        up.submit(rows, cut, model_mask=False)

    # Each tile again with its real neighbours from the levels (keen4-extract
    # context patches); the centre of the result replaces the sheet version.
    for kind in ("tile16", "tile16m"):
        cdir = os.path.join(args.extracted, "context", kind)
        if kind not in kinds or not os.path.isdir(cdir):
            continue
        names = sorted(f for f in os.listdir(cdir) if f.endswith(".png"))
        print(f"{kind}: {len(names)} tiles upscaled with their level neighbours", flush=True)
        for fname in names:
            n = int(fname[:4])
            _, _, rows = read_png(os.path.join(cdir, fname))

            def centre(big, kind=kind, n=n):
                t = 16 * DENSITY
                tile = crop(big, t, t, t, t)
                if kind == "tile16m" and is_empty(tile):
                    return
                name = f"{kind}/{n:04d}.png"
                write_png(os.path.join(args.art, name), t, t, tile)
                manifest[(kind, n)] = f"{kind} {n} {name}"
                original = os.path.join(args.extracted, kind, f"{n:04d}.png")
                review[(kind, n)] = (kind, n, os.path.relpath(original, args.art), name)

            up.submit(rows, centre, model_mask=False)

    for skind in ("sprite", "sprite2"):
        if skind not in kinds:
            continue
        print(f"{skind}: {len(strips[skind])} animation strips", flush=True)
        for strip in strips[skind]:
            frames = [read_png(os.path.join(args.extracted, skind, f"{c:04d}.png")) for c in strip]
            fw, fh = frames[0][0], frames[0][1]
            sw = len(frames) * (fw + STRIP_PAD) + STRIP_PAD
            sheet = [[(0, 0, 0, 0)] * sw for _ in range(fh + 2 * STRIP_PAD)]
            for i, (_, _, rows) in enumerate(frames):
                ox = STRIP_PAD + i * (fw + STRIP_PAD)
                for y in range(fh):
                    sheet[STRIP_PAD + y][ox:ox + fw] = rows[y]

            def split(big, strip=strip, fw=fw, fh=fh, skind=skind):
                for i, c in enumerate(strip):
                    ox = (STRIP_PAD + i * (fw + STRIP_PAD)) * DENSITY
                    img = crop(big, ox, STRIP_PAD * DENSITY, fw * DENSITY, fh * DENSITY)
                    name = f"{skind}/{c:04d}.png"
                    write_png(os.path.join(args.art, name), fw * DENSITY, fh * DENSITY, img)
                    manifest[(skind, c)] = f"{skind} {c} {name}"
                    review[(skind, c)] = (skind, c, os.path.relpath(
                        os.path.join(args.extracted, skind, f"{c:04d}.png"), args.art), name)

            up.submit(sheet, split)

    up.flush()
    os.makedirs(args.art, exist_ok=True)
    with open(os.path.join(args.art, "manifest.txt"), "w") as f:
        f.write(f"# keen4-upscale --method {args.method}" + (f" --model {args.model}" if ai else "") + "\n")
        f.write("# kind number file [anchor ax ay] [bad]  (art at 4x density)\n")
        f.write("\n".join(manifest[k] for k in sorted(manifest)) + "\n")

    with open(os.path.join(args.art, "review.html"), "w") as f:
        f.write("<!doctype html><meta charset=utf-8><title>keen4-upscale review</title><style>"
                "body{background:#222;color:#ddd;font:12px sans-serif}div{display:inline-block;margin:6px;"
                "text-align:center}img{image-rendering:pixelated;background:#707;margin:2px}</style>\n"
                "<p>Original (scaled 4x) and result. Add <b>bad</b> to the asset's line in manifest.txt "
                "to fall back to the built-in upscale.</p>\n")
        for kind, n, original, name in (review[k] for k in sorted(review)):
            f.write(f'<div><img src="{original}" style="zoom:4"><img src="{name}"><br>{kind} {n}</div>\n')

    print(f"wrote {len(manifest)} images, manifest.txt and review.html to {args.art}")


if __name__ == "__main__":
    main()
