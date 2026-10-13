#!/usr/bin/env python3
"""keen4-curate: manages the hand-curated art folder (docs/ART.md).

    python3 keen4_curate.py add KIND NUMBER [NUMBER...] [--from art-ai] [--to art-curated]
    python3 keen4_curate.py remove KIND NUMBER [NUMBER...] [--to art-curated]
    python3 keen4_curate.py list [--to art-curated]

KIND is tile16, tile16m, sprite or sprite2 (Tom's copy, co-op); a NUMBER may be a
range such as 124-131.

`add` copies each asset's current image (and anchor) from the generated set
into the curated folder and lists it in the curated manifest, so an artist
starts from the AI version and paints over it. The game uses the curated
image from then on; hitboxes still come from the original sprite table.
`remove` drops the manifest line (the image stays on disk), so the asset goes
back to the generated set. Only the Python standard library is needed.
Curated art is kept out of the code repository like the generated art.
"""

import argparse
import os
import shutil
import sys

KINDS = ("tile16", "tile16m", "sprite", "sprite2")


def read_manifest(path):
    """Returns (header comment lines, {(kind, number): entry line})."""
    header, entries = [], {}
    if not os.path.exists(path):
        return header, entries
    with open(path, encoding="utf-8") as f:
        for line in f:
            line = line.rstrip("\r\n")
            if not line or line.startswith("#"):
                if not entries:
                    header.append(line)
                continue
            parts = line.split()
            if len(parts) >= 3 and parts[0] in KINDS:
                entries[(parts[0], int(parts[1]))] = line
    return header, entries


def write_manifest(path, header, entries):
    order = {k: i for i, k in enumerate(KINDS)}
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        for line in header or ["# hand-curated art, listed before the generated set",
                               "# kind number file [anchor ax ay] [bad]  (art at 4x density)"]:
            f.write(line + "\n")
        for key in sorted(entries, key=lambda k: (order[k[0]], k[1])):
            f.write(entries[key] + "\n")


def numbers(args):
    for a in args:
        if "-" in a:
            lo, hi = a.split("-", 1)
            yield from range(int(lo), int(hi) + 1)
        else:
            yield int(a)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("command", choices=("add", "remove", "list"))
    ap.add_argument("kind", nargs="?", choices=KINDS)
    ap.add_argument("numbers", nargs="*")
    ap.add_argument("--from", dest="src", default="art-ai", help="generated art folder (default art-ai)")
    ap.add_argument("--to", dest="dst", default="art-curated", help="curated art folder (default art-curated)")
    args = ap.parse_args()

    dst_manifest = os.path.join(args.dst, "manifest.txt")
    header, curated = read_manifest(dst_manifest)

    if args.command == "list":
        for line in curated.values():
            print(line)
        print(f"{len(curated)} curated assets in {args.dst}", file=sys.stderr)
        return 0
    if not args.kind or not args.numbers:
        ap.error("add and remove need KIND and NUMBER")

    if args.command == "remove":
        for n in numbers(args.numbers):
            if curated.pop((args.kind, n), None) is None:
                print(f"{args.kind} {n} is not curated", file=sys.stderr)
        write_manifest(dst_manifest, header, curated)
        return 0

    _, generated = read_manifest(os.path.join(args.src, "manifest.txt"))
    os.makedirs(args.dst, exist_ok=True)
    failed = 0
    for n in numbers(args.numbers):
        key = (args.kind, n)
        if key in curated:
            print(f"{args.kind} {n} is already curated", file=sys.stderr)
            continue
        line = generated.get(key)
        if not line:
            print(f"{args.kind} {n} is not in {args.src}/manifest.txt", file=sys.stderr)
            failed += 1
            continue
        parts = line.split()
        rel = parts[2]
        os.makedirs(os.path.join(args.dst, os.path.dirname(rel)), exist_ok=True)
        if not os.path.exists(os.path.join(args.dst, rel)):
            shutil.copyfile(os.path.join(args.src, rel), os.path.join(args.dst, rel))
        # keep the anchor; drop a "bad" flag, the artist is redoing the asset
        curated[key] = " ".join(p for p in parts if p != "bad")
        print(f"{args.dst}/{rel}")
    write_manifest(dst_manifest, header, curated)
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
