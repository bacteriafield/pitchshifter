## The engine: effects, the Vocal 300 chain, presets and the PortAudio driver.
## Position-independent, so the Lua module (a shared object) can link it too.
file(GLOB CORE_SOURCES CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/audio/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/audio/effects/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/io/*.cpp
)

add_library(PitchShifterCore STATIC ${CORE_SOURCES})
add_library(PitchShifter::Core ALIAS PitchShifterCore)
set_target_properties(PitchShifterCore PROPERTIES POSITION_INDEPENDENT_CODE ON)

## Includes public headers
target_include_directories(PitchShifterCore
    PUBLIC
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(PitchShifterCore
    PUBLIC
        ${PORTAUDIO_TARGET}
)

## C logger
add_library(Logger STATIC logger.c)
add_library(PitchShifter::Logger ALIAS Logger)
target_include_directories(Logger
    PUBLIC
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}
)

## Qt object helpers. Nothing links them yet; they are built so they keep compiling.
if(QT_LIBS)
    add_library(PitchShifterQt STATIC object.cpp)
    add_library(PitchShifter::Qt ALIAS PitchShifterQt)
    target_link_libraries(PitchShifterQt
        PUBLIC
            ${QT_LIBS}
            Logger
    )
endif()
