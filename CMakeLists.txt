cmake_minimum_required(VERSION 3.25)
project(keen4revamped VERSION 1.2.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(KEEN4_WITH_SDL3 "Build the SDL3 platform backend (needed for a window)" ON)
option(KEEN4_BUILD_TESTS "Build the test suite" ON)

if(MSVC)
  set(KEEN4_WARNINGS /W3)
  # Translations are UTF-8 source text.
  add_compile_options(/utf-8)
else()
  set(KEEN4_WARNINGS -Wall -Wextra)
endif()

# Nuked OPL3 (LGPL-2.1), the AdLib emulator.
add_library(nuked_opl3 STATIC third_party/nuked-opl3/opl3.c)
target_include_directories(nuked_opl3 PUBLIC third_party/nuked-opl3)

# Decoders for remastered audio: stb_vorbis (OGG), dr_flac and dr_wav, all
# public domain or MIT.
add_library(audio_codecs STATIC third_party/audio_codecs.c)
target_include_directories(audio_codecs SYSTEM PUBLIC third_party)

# Engine: exe loader, ported id modules, headless platform.
add_library(keen4_engine STATIC
  src/engine/version.cpp
  src/engine/datafiles.cpp
  src/engine/png_writer.cpp
  src/engine/lang.cpp
  src/lang/pt_br.cpp
  src/exe/keen4exe.cpp
  src/platform/platform.cpp
  src/platform/headless/headless_platform.cpp
  src/id/id_ca.cpp
  src/id/id_in.cpp
  src/id/id_rf.cpp
  src/id/id_sd.cpp
  src/id/id_sys.cpp
  src/id/id_us_1.cpp
  src/id/id_us_2.cpp
  src/id/id_vw.cpp
  src/render/art.cpp
  src/render/motion.cpp
  src/render/scene.cpp
  src/audio/audio_file.cpp
  src/audio/remaster_audio.cpp
)
target_include_directories(keen4_engine PUBLIC src)
# stb_image (public domain / MIT) loads PNG art; its own warnings are not ours.
target_include_directories(keen4_engine SYSTEM PRIVATE third_party/stb)
target_link_libraries(keen4_engine PUBLIC nuked_opl3 PRIVATE audio_codecs)
target_compile_options(keen4_engine PRIVATE ${KEEN4_WARNINGS})

# Game: the ported CK_* and K4_* modules.
add_library(keen4_game STATIC
  src/game/ck_main.cpp
  src/game/ck_demo.cpp
  src/game/ck_game.cpp
  src/game/ck_play.cpp
  src/game/ck_text.cpp
  src/game/ck_phys.cpp
  src/game/ck_keen.cpp
  src/game/ck_keen2.cpp
  src/game/ck_coop.cpp
  src/game/coop_split.cpp
  src/game/ck_states.cpp
  src/game/k4_spec.cpp
  src/game/k4_act1.cpp
  src/game/k4_act2.cpp
  src/game/k4_act3.cpp
)
target_include_directories(keen4_game PUBLIC src)
target_link_libraries(keen4_game PUBLIC keen4_engine)
target_compile_options(keen4_game PRIVATE ${KEEN4_WARNINGS})

add_executable(keen4 src/main.cpp)
target_link_libraries(keen4 PRIVATE keen4_game)
target_compile_options(keen4 PRIVATE ${KEEN4_WARNINGS})

if(KEEN4_WITH_SDL3)
  find_package(SDL3 CONFIG QUIET)
  if(NOT SDL3_FOUND)
    include(FetchContent)
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(SDL3
      GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
      GIT_TAG release-3.2.24
      GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(SDL3)
  endif()
  target_sources(keen4 PRIVATE src/platform/sdl3/sdl_platform.cpp src/platform/sdl3/remaster_renderer.cpp)
  target_link_libraries(keen4 PRIVATE SDL3::SDL3)
  target_compile_definitions(keen4 PRIVATE KEEN4_WITH_SDL3=1)
  if(WIN32)
    set_target_properties(keen4 PROPERTIES WIN32_EXECUTABLE ON)
    target_sources(keen4 PRIVATE packaging/windows/keen4.rc)
  endif()
endif()

# Art pipeline: exports the original graphics as PNG for artists and keen4-upscale.
add_executable(keen4-extract tools/keen4-extract/extract.cpp)
target_link_libraries(keen4-extract PRIVATE keen4_engine)
target_compile_options(keen4-extract PRIVATE ${KEEN4_WARNINGS})

if(KEEN4_BUILD_TESTS)
  enable_testing()
  add_executable(keen4_tests tests/test_main.cpp)
  target_link_libraries(keen4_tests PRIVATE keen4_game)
  add_test(NAME keen4_tests COMMAND keen4_tests)
  add_executable(keen4_audio_tests tests/test_audio.cpp)
  target_link_libraries(keen4_audio_tests PRIVATE keen4_game)
  add_test(NAME keen4_audio_tests COMMAND keen4_audio_tests)
  # Co-op scenarios run the game headless; they need the game data (KEEN4_DATA)
  # and are skipped without it.
  add_executable(keen4_coop_tests tests/test_coop.cpp)
  target_compile_options(keen4_coop_tests PRIVATE ${KEEN4_WARNINGS})
  target_compile_definitions(keen4_coop_tests PRIVATE KEEN4_BINARY="$<TARGET_FILE:keen4>"
                                                      KEEN4_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
  add_dependencies(keen4_coop_tests keen4)
  add_test(NAME keen4_coop_tests COMMAND keen4_coop_tests)
  set_tests_properties(keen4_coop_tests PROPERTIES SKIP_RETURN_CODE 77)
endif()
