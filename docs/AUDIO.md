# Audio and remastered sound

The original sound code runs unchanged: an emulated OPL2 (Nuked OPL3) plays the AdLib
music and effects, and a square wave plays the PC speaker effects. Milestone 7 adds
optional recordings that replace songs and effects one at a time, the same way curated art
replaces tiles and sprites.

| | Original | Remastered |
| --- | --- | --- |
| Music | AdLib songs (IMF) through the emulated OPL2 | `music N` in `audio/manifest.txt`, else the original |
| Effects | AdLib or PC speaker, as chosen in the sound menu | `sound N` in `audio/manifest.txt`, else the original |
| Output | 48 kHz stereo float | the same, mixed with the original where nothing is listed |

Remastered sound is off unless the game is started with `--remastered-audio`; then
**Shift+F11** switches between remastered and original sound while playing.

## Options

- `--remastered-audio`: play the recordings listed in `audio/manifest.txt`.
- `--audio DIR`: the same with another folder.
- `--music-volume N`, `--sfx-volume N`: 0 to 100, applied to the original and remastered
  sound alike. Music and effects have separate volumes because each runs on its own
  emulated OPL2; the original keeps effects on channel 0 and music on channels 1 to 8, so the
  two chips together sound like one card.
- The control panel still decides whether there is music and which effects device plays:
  "No music" silences remastered music too, and remastered effects replace both AdLib and
  PC speaker effects.

## manifest.txt

```
# kind number file [loop SECONDS] [volume V]
music 0 music/title.ogg loop 7.25
music 3 music/forest.flac
sound 2 sfx/jump.wav volume 0.8
```

- `music N`: a recording for song number N (0 to 5). Songs loop like the original; `loop`
  sets where the loop restarts, in seconds from the start (default 0), so a track can have an
  intro that plays once. The game prints `audio: song N` when a song starts, so you can find
  which song a level uses.
- `sound N`: a recording for sound number N (0 to 51; the names are the `SND_` constants in
  `src/game`, for example 2 jump, 3 land, 4 fire, 7 pogo bounce, 23 Keen dies).
- `volume V`: a gain for that file (default 1), to level recordings against each other.
- Files can be OGG Vorbis, FLAC or WAV, at any sample rate (resampled to 48 kHz), mono or
  stereo. Songs are streamed from disk; effects are decoded when the game starts.
- A file that cannot be read is reported on stderr and the original plays instead.

## What stays the same

The original sound code keeps running underneath a recording, so the game sees exactly the
same sound timing: `SD_SoundPlaying` reports the original effect until its original length
has passed, and a sound of lower priority is still refused while it plays. Only what is
heard changes. Like the original, one effect plays at a time: a new effect replaces the
recording of the previous one.

Recordings must be our own or properly licensed: the remastered music and effects are new
works, never rips of the originals, and like the art they are kept out of this repository
(`/audio/` is ignored).
