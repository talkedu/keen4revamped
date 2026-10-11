# Decompilation tools

Copied from the project's working folder on 2026-10-11. Run them against your own copy of the
game; keep every output out of the repository (`extracted/` and `listings/` are git-ignored).

| Script | Usage | Output |
| --- | --- | --- |
| `unlzexe.py` | `unlzexe.py KEEN4E.EXE extracted/KEEN4E_UNP.EXE` | Unpacked MZ executable |
| `extract_heads.py` | `extract_heads.py extracted/KEEN4E_UNP.EXE extracted/` | AUDIOHED, EGAHEAD, EGADICT, MAPHEAD `.CK4` tables |
| `segmap.py` | `segmap.py extracted/KEEN4E_UNP.EXE` | Code segment map from far-call relocations |
| `disasm.py` | `disasm.py extracted/KEEN4E_UNP.EXE listings/` | Per-segment listings (needs `pip install capstone`) |
| `findheads.py`, `testdict.py` | exploratory | Used to locate and check the header tables |
