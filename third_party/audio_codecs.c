/* Implementations of the vendored audio decoders, built as their own library
   so their warnings stay out of ours. */
#define DR_FLAC_IMPLEMENTATION
#define DR_FLAC_NO_OGG
#include "dr_libs/dr_flac.h"

#define DR_WAV_IMPLEMENTATION
#include "dr_libs/dr_wav.h"

#include "stb/stb_vorbis.c"
