cmake_minimum_required(VERSION 3.25)

# The version is not kept in the source tree. The release workflow passes the
# version it is about to tag (-DKEEN4_VERSION=x.y.z). Every other build (pull
# requests, branches, local builds) is the next version plus "-SNAPSHOT": the
# latest v* git tag bumped by KEEN4_VERSION_BUMP, as the release workflow will
# bump it when the pull request merges. 0.0.1-SNAPSHOT without any tag.
set(KEEN4_VERSION "" CACHE STRING "Release version, x.y.z (empty: next version + -SNAPSHOT)")
set(KEEN4_VERSION_BUMP "patch" CACHE STRING "How a snapshot build bumps the latest v* tag: major, minor or patch")
set_property(CACHE KEEN4_VERSION_BUMP PROPERTY STRINGS major minor patch)
if(KEEN4_VERSION)
  if(NOT KEEN4_VERSION MATCHES "^([0-9]+)\.([0-9]+)\.([0-9]+)$")
    message(FATAL_ERROR "KEEN4_VERSION must look like 1.2.3, got '${KEEN4_VERSION}'")
  endif()
  set(_keen4_version "${KEEN4_VERSION}")
  set(KEEN4_VERSION_LABEL "${KEEN4_VERSION}")
else()
  # the highest tag, as the release workflow picks it ("v1.2" counts as 1.2.0)
  execute_process(
    COMMAND git tag --list "v*" --sort=-v:refname
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    OUTPUT_VARIABLE _keen4_tags ERROR_QUIET)
  string(REPLACE "\n" ";" _keen4_tags "${_keen4_tags}")
  set(_keen4_major 0)
  set(_keen4_minor 0)
  set(_keen4_patch 0)
  foreach(_keen4_tag IN LISTS _keen4_tags)
    if(_keen4_tag MATCHES "^v([0-9]+)\.([0-9]+)(\.([0-9]+))?$")
      set(_keen4_major "${CMAKE_MATCH_1}")
      set(_keen4_minor "${CMAKE_MATCH_2}")
      if(NOT CMAKE_MATCH_4 STREQUAL "")
        set(_keen4_patch "${CMAKE_MATCH_4}")
      endif()
      break()
    endif()
  endforeach()
  if(KEEN4_VERSION_BUMP STREQUAL "major")
    math(EXPR _keen4_major "${_keen4_major} + 1")
    set(_keen4_minor 0)
    set(_keen4_patch 0)
  elseif(KEEN4_VERSION_BUMP STREQUAL "minor")
    math(EXPR _keen4_minor "${_keen4_minor} + 1")
    set(_keen4_patch 0)
  elseif(KEEN4_VERSION_BUMP STREQUAL "patch")
    math(EXPR _keen4_patch "${_keen4_patch} + 1")
  else()
    message(FATAL_ERROR "KEEN4_VERSION_BUMP must be major, minor or patch, got '${KEEN4_VERSION_BUMP}'")
  endif()
  set(_keen4_version "${_keen4_major}.${_keen4_minor}.${_keen4_patch}")
  set(KEEN4_VERSION_LABEL "${_keen4_version}-SNAPSHOT")
endif()

project(keen4revamped VERSION ${_keen4_version} LANGUAGES C CXX)
message(STATUS "keen4revamped version ${KEEN4_VERSION_LABEL}")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(KEEN4_WITH_SDL3 "Build the SDL3 platform backend (needed for a window)" ON)
option(KEEN4_BUILD_TESTS "Build the test suite" ON)

# In-game updater (main menu, CHECK FOR UPDATES): the GitHub OAuth app players
# sign in with (its client id is public; empty turns the updater off) and the
# repository whose releases it installs.
set(KEEN4_GITHUB_CLIENT_ID "" CACHE STRING "Client id of the GitHub OAuth app the updater signs in with")
set(KEEN4_UPDATE_REPO "talkedu/keen4revamped" CACHE STRING "owner/name of the repository the updater installs releases from")

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
  src/engine/gamedata_search.cpp
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
  src/update/json.cpp
  src/update/http.cpp
  src/update/github.cpp
  src/update/install.cpp
)
target_include_directories(keen4_engine PUBLIC src)
# stb_image (public domain / MIT) loads PNG art; its own warnings are not ours.
target_include_directories(keen4_engine SYSTEM PRIVATE third_party/stb)
target_link_libraries(keen4_engine PUBLIC nuked_opl3 PRIVATE audio_codecs)
if(WIN32)
  target_link_libraries(keen4_engine PRIVATE advapi32)  # Steam location in the registry
endif()
target_compile_options(keen4_engine PRIVATE ${KEEN4_WARNINGS})
set_source_files_properties(src/engine/version.cpp PROPERTIES
  COMPILE_DEFINITIONS "KEEN4_VERSION=\"${KEEN4_VERSION_LABEL}\"")
if(KEEN4_VERSION)
  # a release build (see the top): it starts in full screen until the player changes it
  set_property(SOURCE src/engine/version.cpp APPEND PROPERTY COMPILE_DEFINITIONS KEEN4_RELEASE_BUILD)
endif()
set_source_files_properties(src/update/github.cpp PROPERTIES
  COMPILE_DEFINITIONS "KEEN4_GITHUB_CLIENT_ID=\"${KEEN4_GITHUB_CLIENT_ID}\";KEEN4_UPDATE_REPO=\"${KEEN4_UPDATE_REPO}\"")
# The updater talks HTTPS through WinHTTP on Windows and through the system's
# libcurl, loaded at run time, elsewhere; it works in a background thread.
find_package(Threads REQUIRED)
target_link_libraries(keen4_engine PRIVATE Threads::Threads)
if(WIN32)
  target_link_libraries(keen4_engine PRIVATE winhttp shell32 ole32)
else()
  target_link_libraries(keen4_engine PRIVATE ${CMAKE_DL_LIBS})
endif()

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
  target_sources(keen4 PRIVATE src/platform/sdl3/sdl_platform.cpp src/platform/sdl3/remaster_renderer.cpp
                               src/platform/sdl3/fb_output.cpp)
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
  add_executable(keen4_update_tests tests/test_update.cpp)
  target_link_libraries(keen4_update_tests PRIVATE keen4_engine)
  target_compile_options(keen4_update_tests PRIVATE ${KEEN4_WARNINGS})
  add_test(NAME keen4_update_tests COMMAND keen4_update_tests)
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
