## Notes

Thanks to id Software and Apogee for Commander Keen 4: Secret of the Oracle, to
[Omnispeak](https://github.com/sulix/omnispeak) and K1n9_Duk3's reconstructed Keen 4-6
source, which were references while decompiling, and to the Keen Dreams source release.
Commander Keen 4 Revamped is a C++ port of the game by
[talkedu](https://github.com/talkedu/keen4revamped), with smooth scrolling, a 16:9 view,
optional upscaled art and remastered audio, and two-player co-op. It is an unofficial fan
project and contains no game data.

Get Commander Keen from:
[Steam](https://store.steampowered.com/app/9180/Commander_Keen/)

#### Game files needed

Copy these from your copy of Keen 4 (v1.4 EGA) into `keen4revamped/gamedata`:

- KEEN4E.EXE
- AUDIO.CK4, EGAGRAPH.CK4, GAMEMAPS.CK4 and the other .CK4 files

The Steam release has them in `steamapps/common/Commander Keen/base4`. Copying the whole
`base4` folder into `gamedata` works too.

Saves and settings go in `keen4revamped/saves`. On a 4:3 or square screen the first run
starts in the original 4:3 view, which fills the screen; VIDEO in the menu switches to the
16:9 view.

</br>

## Controls

| Button | Action |
|--|--|
|D-pad/Left Analog|Move, look up/down|
|A|Jump|
|B|Pogo|
|X/Y|Fire|
|Select|Status window|
|Start|Menu|
|Select + Start|Quit|

In menus: d-pad moves, A selects, B or Start goes back.

</br>

## Compile

```shell
git clone https://github.com/talkedu/keen4revamped
cd keen4revamped
sh packaging/anbernic/fetch-toolchain.sh
cmake --preset anbernic
cmake --build --preset anbernic
sh packaging/anbernic/make-package.sh --portmaster portmaster build/anbernic keen4revamped.zip
```
