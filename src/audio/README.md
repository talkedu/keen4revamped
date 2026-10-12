# Audio

OPL2 emulation (Nuked OPL3, vendored into third_party/), IMF music player and PC speaker
synthesis live in `src/id/id_sd.cpp`. This folder holds the remastered sound
(docs/AUDIO.md): `audio_file` decodes OGG, FLAC and WAV and resamples them, and
`remaster_audio` reads `audio/manifest.txt` and plays recordings in place of songs and
effects while the original sound code keeps its timing.
