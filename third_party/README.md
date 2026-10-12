# Third-party code

Vendored dependencies go here, each in its own folder with its licence file (or, for
single-file libraries, the licence at the end of the file).

| Library | Version | Licence | Used for |
| --- | --- | --- | --- |
| nuked-opl3 | 1.8 | LGPL-2.1 | AdLib (OPL2) emulation |
| stb/stb_image.h | 2.30 | public domain or MIT | PNG art |
| stb/stb_vorbis.c | 1.22 | public domain or MIT | remastered audio: OGG Vorbis |
| dr_libs/dr_flac.h | 0.13.4 | public domain or MIT-0 | remastered audio: FLAC |
| dr_libs/dr_wav.h | 0.14.6 | public domain or MIT-0 | remastered audio: WAV |

`audio_codecs.c` builds the three audio decoders as one library.
