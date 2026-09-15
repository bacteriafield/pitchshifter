cmake_minimum_required(VERSION 3.16)
project(PitchShifter VERSION 0.0.1 DESCRIPTION "An vocal 300 like" LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Force Homebrew prefix for Apple Silicon
if(APPLE)
    set(HOMEBREW_PREFIX "/opt/homebrew")
    include_directories(${HOMEBREW_PREFIX}/include)
    link_directories(${HOMEBREW_PREFIX}/lib)
endif()

set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)
include(GNUInstallDirs)

## Path to source-code
set(SRC_DIR ${CMAKE_SOURCE_DIR}/src)
set(CORE_DIR ${SRC_DIR}/core)

#############################################################################
##
##   cmake -S . -B build && cmake --build build     build everything
##   ctest --test-dir build --output-on-failure     run the checks
##   cmake --build build --target run-gui           open the front panel
##
##   src/core   PitchShifterCore   engine, effects, presets, PortAudio driver
##   src/cli    PitchShifter       command-line program
##   src/gui    pcore.so           the engine as a Lua module, + pitchshifter-gui
##
#############################################################################

option(ENABLE_QT   "Build the Qt object helpers (src/core/object.cpp)" ON)
option(BUILD_CLI   "Build the PitchShifter command-line program" ON)
option(BUILD_GUI   "Build the Lua/IUP front panel (needs Lua 5.4 headers)" ON)
option(BUILD_TESTS "Register the ctest checks" ON)

set(VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(VERSION_PATCH ${PROJECT_VERSION_PATCH})
set(DISPLAY_VERSION "${PROJECT_VERSION}")

execute_process(
    COMMAND git rev-parse --short HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_REVISION
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)
if(NOT GIT_REVISION)
    set(GIT_REVISION "unknown")
endif()

#############################################################################
## Dependencies

## PortAudio: pkg-config first, cmake/findPortAudio.cmake as the fallback
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(PORTAUDIO QUIET IMPORTED_TARGET portaudio-2.0)
endif()

if(TARGET PkgConfig::PORTAUDIO)
    set(PORTAUDIO_TARGET PkgConfig::PORTAUDIO)
else()
    include(findPortAudio)
    set(PORTAUDIO_TARGET PortAudio::PortAudio)
endif()

if(NOT TARGET ${PORTAUDIO_TARGET})
    message(FATAL_ERROR "PortAudio not found! Install with 'sudo apt install portaudio19-dev' or 'brew install portaudio'")
endif()
set(HAVE_PORTAUDIO 1)

## Qt: only the object helpers use it, and nothing links those yet
if(ENABLE_QT)
    find_package(Qt6 COMPONENTS Core QUIET)
    if(Qt6_FOUND)
        set(QT_LIBS Qt6::Core)
    else()
        find_package(Qt5 COMPONENTS Core QUIET)
        if(Qt5_FOUND)
            set(QT_LIBS Qt5::Core)
        else()
            message(STATUS "Qt not found -- skipping the Qt object helpers")
        endif()
    endif()
endif()

## Generated after the dependencies, so the HAVE_* defines are right
configure_file(
    ${CORE_DIR}/config.h.in
    ${CORE_DIR}/config.h
    @ONLY
)

#############################################################################
## Targets

if(BUILD_TESTS)
    enable_testing()
endif()

add_subdirectory(src/core)

if(BUILD_CLI)
    add_subdirectory(src/cli)
endif()

if(BUILD_GUI)
    add_subdirectory(src/gui)
endif()

#############################################################################
## Install headers

install(DIRECTORY ${SRC_DIR}/core/audio/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/pitchshifter/core/audio
    FILES_MATCHING PATTERN "*.h"
)

install(FILES ${CORE_DIR}/config.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/pitchshifter
)
