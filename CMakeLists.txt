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

## Path to source-code
set(SRC_DIR ${CMAKE_SOURCE_DIR}/src)
set(CORE_DIR ${SRC_DIR}/core)
set(CLI_DIR ${SRC_DIR}/cli)

## Module
list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)

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

option(ENABLE_QT "Enable Qt Core dependency" ON)
option(ENABLE_PORTAUDIO "Enable PortAudio backend" ON)

set(MAX_BUFFER_SIZE 4096)
set(MAX_FX 16)
set(HAVE_PORTAUDIO 0)

configure_file(
    ${CORE_DIR}/config.h.in
    ${CORE_DIR}/config.h
    @ONLY
)

# Disponibilizar o diretório com o header gerado
include_directories(${CORE_DIR})

#############################################################################

## Fonts
file(GLOB_RECURSE CORE_SOURCES ${CORE_DIR}/*.cpp ${CORE_DIR}/*.c)
set(SOURCES
    ${CLI_DIR}/main.cpp
    ${CORE_SOURCES}
)

find_package(Qt6 COMPONENTS Core QUIET)
if (NOT Qt6_FOUND)
    message(STATUS "Qt6 not found — trying Qt5...")
    find_package(Qt5 COMPONENTS Core REQUIRED)
    set(QT_LIBS Qt5::Core)
else()
    message(STATUS "Qt6 found!")
    set(QT_LIBS Qt6::Core)
endif()

find_package(PkgConfig REQUIRED)
pkg_check_modules(PORTAUDIO portaudio-2.0)

if (PORTAUDIO_FOUND)
    message(STATUS "Using PortAudio by pkg-config")
    set(HAVE_PORTAUDIO 1)
else()
    message(WARNING "pkg-config not found PortAudio — trying manually...")
    find_path(PORTAUDIO_INCLUDE_DIR portaudio.h)
    find_library(PORTAUDIO_LIBRARY NAMES portaudio)
    if(PORTAUDIO_INCLUDE_DIR AND PORTAUDIO_LIBRARY)
        set(HAVE_PORTAUDIO 1)
    endif()
endif()

if (NOT PORTAUDIO_INCLUDE_DIR AND NOT PORTAUDIO_INCLUDE_DIRS)
    message(FATAL_ERROR "PortAudio not found! Install with 'sudo apt install portaudio19-dev' or 'brew install portaudio'")
endif()


##################Linking#################################################
add_executable(${PROJECT_NAME} ${SOURCES})

## Include directories
target_include_directories(${PROJECT_NAME}
    PRIVATE
        ${SRC_DIR}
        ${CORE_DIR}  # ← config.h gen
        ${PORTAUDIO_INCLUDE_DIRS}
        ${PORTAUDIO_INCLUDE_DIR}
)

## Link libraries
target_link_libraries(${PROJECT_NAME}
    PRIVATE
        ${QT_LIBS}
        ${PORTAUDIO_LIBRARIES}
        ${PORTAUDIO_LIBRARY}
)

## Install
install(TARGETS ${PROJECT_NAME}
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

install(DIRECTORY ${SRC_DIR}/core/audio/
    DESTINATION include/pitchshifter/core/audio
    FILES_MATCHING PATTERN "*.h"
)

install(FILES ${CORE_DIR}/config.h
    DESTINATION include/pitchshifter
)
#############################################################################
## Effect self-checks: cmake -DBUILD_TESTS=ON .. && cmake --build . --target effects_check
option(BUILD_TESTS "Build the effect self-checks" OFF)
if(BUILD_TESTS)
    file(GLOB EFFECT_SOURCES ${CORE_DIR}/audio/effects/*.cpp)
    add_executable(effects_check ${CMAKE_SOURCE_DIR}/tests/effects_check.cpp ${EFFECT_SOURCES}
        ${CORE_DIR}/audio/vocal_chain.cpp ${CORE_DIR}/audio/preset.cpp)
    target_include_directories(effects_check PRIVATE ${SRC_DIR} ${CORE_DIR})
endif()
