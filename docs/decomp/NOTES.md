# Keen 4 decompilation notes

Goal (Eduardo, 2026-10-11): decompile Keen4e.exe ourselves and write new source from it.

## Step 1: unpacked executable

- `base4/Keen4e.exe` is LZEXE 0.91 packed (102.6 KB). `tools/unlzexe.py` unpacks it to `extracted/KEEN4E_UNP.EXE` (251,712-byte load image, 2,931 relocations, entry CS:IP 0000:0000, SS:SP 3D6C:0080).
- Compiler: Borland C++ 1991 runtime ("Borland C++ - Copyright 1991 Borland Intl."). Medium memory model (far calls between modules).
- Version: 4,752 EGAHEAD entries = 4,751 graphics chunks, which matches Keen 4 v1.4 EGA. Not confirmed by a version string.

## Step 2: data header tables (offsets into the load image)

| Table | Offset | Size | Check |
| --- | --- | --- | --- |
| AUDIOHED | 0x20DF0 | 652 B (163 x 4) | last entry = Audio.ck4 size 33,325 |
| EGAHEAD | 0x21080 | 14,256 B (4,752 x 3) | last entry = Egagraph.ck4 size 520,581 |
| MAPHEAD | 0x24830 | 23,408 B (RLEW tag 0xABCD + map pointers + tileinfo) | first map pointer 5,724 |
| EGADICT | 0x358F6 | 1,024 B (255 Huffman nodes) | decodes 100/100 sampled length-prefixed chunks |

Dumped to `extracted/*.CK4` by `tools/extract_heads.py`. These are the files a modern loader needs next to the three .ck4 data files.

## Step 3: segment map

`tools/segmap.py` recovers 24 code segments from far-call relocations (`extracted/segmap.txt`), roughly one per source module, code ending near 0x1ED30. Functions only called near inside their own segment are not counted yet.

## Next

1. Disassemble each code segment (ndisasm -b16, or capstone) into per-segment listings with function boundaries.
2. Name the Borland C runtime functions first, then the ID engine (ID_MM, ID_CA, ID_VW, ID_RF, ID_SD, ID_IN, ID_US) using id's GPL releases of the same engine (Wolfenstein 3D, Catacomb 3-D, Keen Dreams) as naming references.
3. Write C++ function by function, checking behaviour against the original with demo playback in DOSBox.

Original game files and the unpacked exe are copyrighted: keep them out of any public repository.

## Step 4: listings and module map (2026-10-11)

- `tools/disasm.py` (needs `pip install capstone`) writes `listings/seg_XXXX.asm` per code segment with function labels, far-call names and DGROUP string annotations.
- DGROUP = 0x2EE7 (from the c0 startup `mov dx,2EE7h`).
- `MODULES.md` maps the 24 segments to source modules. 20 functions named from their own error strings so far.
