## Main Executable
add_executable(PitchShifter main.cpp)

## Link with core lib
target_link_libraries(PitchShifter
    PRIVATE
        PitchShifter::Core
        PitchShifter::Logger
)

## Install
install(TARGETS PitchShifter RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
