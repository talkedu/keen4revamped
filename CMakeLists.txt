cmake_minimum_required(VERSION 3.25)
project(keen4revamped VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(KEEN4_WITH_SDL3 "Build the SDL3 platform backend" OFF)
option(KEEN4_BUILD_TESTS "Build the test suite" ON)

if(MSVC)
  set(KEEN4_WARNINGS /W4)
else()
  set(KEEN4_WARNINGS -Wall -Wextra -Wconversion)
endif()

add_library(keen4_engine STATIC
  src/engine/version.cpp
  src/platform/headless/headless_platform.cpp
)
target_include_directories(keen4_engine PUBLIC src)
target_compile_options(keen4_engine PRIVATE ${KEEN4_WARNINGS})

if(KEEN4_WITH_SDL3)
  find_package(SDL3 CONFIG QUIET)
  if(NOT SDL3_FOUND)
    include(FetchContent)
    FetchContent_Declare(SDL3
      GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
      GIT_TAG release-3.2.24)
    FetchContent_MakeAvailable(SDL3)
  endif()
  target_link_libraries(keen4_engine PUBLIC SDL3::SDL3)
  target_compile_definitions(keen4_engine PUBLIC KEEN4_WITH_SDL3=1)
endif()

add_executable(keen4 src/main.cpp)
target_link_libraries(keen4 PRIVATE keen4_engine)
target_compile_options(keen4 PRIVATE ${KEEN4_WARNINGS})

if(KEEN4_BUILD_TESTS)
  enable_testing()
  add_executable(keen4_tests tests/test_main.cpp)
  target_link_libraries(keen4_tests PRIVATE keen4_engine)
  add_test(NAME keen4_tests COMMAND keen4_tests)
endif()
