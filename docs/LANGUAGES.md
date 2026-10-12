# Languages

The game can show its text in English (the original) or Brazilian Portuguese. The LANGUAGE
item of the main menu cycles through the languages; the choice is saved at the end of
`CONFIG.CK4` (older files without it keep working) and `--lang en|pt-BR` overrides it for
one run.

## How a translation works

The original text is never in this repository: menus, dialogs, level names and messages are
read from the player's `KEEN4E.EXE`, and the help topics, story and ending are text articles
in `EGAGRAPH.CK4`. A translation in [src/lang](../src/lang) therefore names what it replaces:

- an exe string by its address in the unpacked load image, written `DG(ofs)` for DGROUP or
  `FAR(seg, ofs)` for a far segment, with the English in a comment;
- a string the port prints from its own source by its English text;
- a whole article by its graphics chunk (4739 ComputerWrist help, 4741 game play, 4740 story,
  4745 ordering, 4742 about Id, 4743 ending), keeping the original's `^P` pages and `^G`/`^T`
  picture commands.

[src/engine/lang.cpp](../src/engine/lang.cpp) swaps the text in where it is drawn
(`US_Print`, `US_CPrint`, the control panel and the help layout), so the game code keeps
passing the original strings. Translations are UTF-8 and become ISO-8859-1 when loaded,
because the game draws one byte per character.

The original fonts stop at ASCII. ID_VW draws the accented letters of ISO-8859-1 (0xC0-0xFF)
from the plain letter plus a mark (acute, grave, circumflex, tilde, diaeresis, ring, cedilla);
marks above capitals rise up to two rows over the font's first row.

Windows and panel items are sized for the English text, so keep translated lines about as wide
as the original. The widest are the 160-pixel talk and level-entry windows, the panel items and
dialogs, and the 64-pixel boxes of the status window. Help pages hold 18 lines; text that
does not fit on a page is cut, so split long pages with an extra `^P` rather than letting them
overflow.

## Adding a language

1. Add it to `Language` and to `Data`, `Parse` and `Code` in `lang.h`/`lang.cpp`.
2. Copy `src/lang/pt_br.cpp`, translate it and add it to `keen4_engine` in `CMakeLists.txt`.
3. Run the game with `--lang` and look at the main menu cards, the help topics, a level's
   entry window (`/TEDLEVEL n`) and the status window (Enter on the map).

## Text drawn into pictures

These pictures carry English baked into the art and still show it in every language.
Translating them needs per-language pictures, which nothing loads yet: menus and help are
shown from the classic frame, outside the art manifest of [ART.md](ART.md).

| Chunk | What it says |
|---|---|
| 6 | Help menu: COMPUTER WRIST, THE GAME, THE STORY, ORDER INFO, ABOUT ID, MOVE, SELECT, ESC BACKS OUT |
| 83 | Help page frame: PAGE, ESC EXITS |
| 43 | END OF TEXT |
| 86 | Here's some exciting shots from Keen 5! (ordering pages) |
| 88-99, 101 | Control panel card titles: MAIN MENU, NEW GAME MENU, LOAD MENU, SAVE MENU, CONFIGURE MENU, SOUND EFFECTS MENU, MUSIC MENU, KEYBOARD MENU, MOVEMENT SETUP, BUTTON SETUP, JOYSTICK MENU, OPTIONS MENU, QUIT |
| 100 | Paddle War! |
| 103 | Control panel frame: Press F1 for Help and Order Info |
| 109 | Title screen: Commander Keen in Goodbye, Galaxy!, Press F1 for Help |
| 104-107 | Intro credits (the intro is not ported yet) |
| 72, 75, 78 | Ending: YOU ARE HERE / THEY ARE HERE, KEEP OUT, To be continued... |
| 8x8 tiles 72-91 | Status window: PRESS A KEY |

Logos and names (Id Software, Visa, MasterCard), the ZAP! sound effect, the Esc and Enter key
caps of the help pages and the alien writing in the ending are left as they are. Some map
tiles carry words too (the NO DIVING sign).
