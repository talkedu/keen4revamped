# Code segment to module map (Keen4e.exe, unpacked)

Inferred 2026-10-11 from far-call relocations and the error/text strings each segment references
(`tools/disasm.py`, `listings/summary.json`). "Evidence" is the deciding string. Confidence is mine.

| Segment | Bytes | Functions | Module (inferred) | Evidence | Confidence |
| --- | ---: | ---: | --- | --- | --- |
| 0000 | 14,288 | 172 | Borland C0 startup + C runtime | entry point, "Null pointer assignment", "%s: %s" | High |
| 037D | 1,792 | 9 | CK_MAIN (startup) | "Combining EGA1.CK4 and EGA2.CK4" | Medium |
| 03ED | 7,536 | 22 | CK_DEMO (title, story) | "Id Software - '91", story text | Medium |
| 05C4 | 3,984 | 17 | CK_GAME | "Not enough memory to load game!", "Game Over!" | High |
| 06BD | 9,600 | 24 | CK_PLAY (play loop, debug keys) | "Inactive Objects:", "Memory Usage" | Medium |
| 0915 | 3,184 | 15 | CK_TEXT (help/story pages) | "PageLayout:", "CacheLayoutGraphics:" | High |
| 09DC | 6,720 | 39 | CK_PHYS + shared actor code | "FullClipToWalls: Bad obclass" | High |
| 0B80 | 8,432 | 43 | CK_KEEN | "Keyholder points to a bad spot!" | Medium |
| 0D8F | 4,096 | 22 | CK_KEEN2 / CK_OBJ | "SpawnShot: Bad dir!" | Medium |
| 0E8F | 4,496 | 12 | K4_SPEC | "Princess Lindsey says:", "I can't swim!" | High |
| 0FA8 | 3,456 | 44 | K4_ACT1 | no strings; actor code by position | Low |
| 1080 | 4,640 | 41 | K4_ACT2 | no strings | Low |
| 11A2 | 4,160 | 38 | K4_ACT3 | no strings | Low |
| 12A6 | 7,072 | 37 | ID_CA | CA_CacheMap, CA_UpLevel, CAL_CacheSprite | High |
| 1460 | 4,064 | 37 | ID_IN | "Demo playback exceeded", "Demo buffer overflow" | High |
| 155E | 3,936 | 19 | ID_MM | MM_GetPtr, MM_FreePtr, "EMMXXXX0" | High |
| 1654 | 7,456 | 34 | ID_RF | RF_NewMap, RF_PlaceSprite, RF_SetScrollBlock | High |
| 1826 | 1,248 | 3 | ID_RF_A (asm) | 3 routines, no strings, follows ID_RF | Low |
| 1874 | 3,024 | 35 | ID_SD | "SDL_ALPlaySound() - Bad instrument" | High |
| 1931 | 5,504 | 35 | ID_US_1 | "CONFIG.CK4", "Device Error" | Medium |
| 1A89 | 10,128 | 54 | ID_US_2 (control panel) | "YOU'RE IN A GAME", "PRESS ANY KEY" | High |
| 1D02 | 64 | 2 | ID_US_A (asm, RNG) | 2 tiny routines | Low |
| 1D06 | 4,160 | 46 | ID_VW | "Improper video card!" | High |
| 1E0A | 3,216 | 19 | ID_VW_AE / ID_VH (asm) | no deciding string | Low |

Code segments end at 0x1ED30. DGROUP (data) is segment 0x2EE7. Far data segments 0x1ED3-0x2B63 hold
tables such as AUDIOHED (0x20DF), EGAHEAD (0x2108) and MAPHEAD (0x2483).

20 functions are already named from their own error strings (listings/summary.json "names");
the remaining ~800 entry points are `sub_SSSS_OOOO`.
