## Lua/IUP front panel: the engine as a Lua module (pcore.so), driven by main.lua.
##
##   build/bin/pitchshifter-gui     run it from the build tree
##   cmake --build build --target run-gui

## Lua 5.4 headers. The module is loaded into the interpreter, so they must match it.
find_path(LUA_INCLUDE_DIR lua.h PATH_SUFFIXES lua5.4 lua54 lua-5.4 lua)
if(LUA_INCLUDE_DIR)
    file(STRINGS ${LUA_INCLUDE_DIR}/lua.h LUA_VERSION_NUM_LINE REGEX "^#define LUA_VERSION_NUM")
endif()

if(NOT LUA_VERSION_NUM_LINE MATCHES "504")
    message(WARNING "Lua 5.4 headers not found (set LUA_INCLUDE_DIR) -- the GUI is not built")
    return()
endif()

## The interpreter that runs the panel: the first "lua" or "lua5.4" that is 5.4
if(NOT LUA_EXECUTABLE)
    foreach(candidate lua lua5.4)
        find_program(LUA_CANDIDATE_${candidate} ${candidate})
        if(LUA_CANDIDATE_${candidate})
            execute_process(
                COMMAND ${LUA_CANDIDATE_${candidate}} -e "io.write(_VERSION)"
                OUTPUT_VARIABLE lua_version
                ERROR_QUIET
            )
            if(lua_version STREQUAL "Lua 5.4")
                set(LUA_EXECUTABLE ${LUA_CANDIDATE_${candidate}} CACHE FILEPATH "Lua 5.4 interpreter that runs the GUI")
                break()
            endif()
        endif()
    endforeach()
endif()

if(LUA_EXECUTABLE)
    # Only looks for the file: actually loading iuplua opens a display.
    execute_process(
        COMMAND ${LUA_EXECUTABLE} -e "if not package.searchpath('iuplua', package.cpath) then os.exit(1) end"
        RESULT_VARIABLE iuplua_missing
        OUTPUT_QUIET ERROR_QUIET
    )
    if(iuplua_missing)
        message(WARNING "${LUA_EXECUTABLE} cannot find iuplua -- pcore builds, but the panel needs IUP to run")
    endif()
else()
    message(WARNING "No Lua 5.4 interpreter found -- pcore builds, but there is nothing to run the panel with")
endif()

#############################################################################
## pcore.so

add_library(pcore MODULE pcore_lua.cpp)
set_target_properties(pcore PROPERTIES PREFIX "")
target_include_directories(pcore PRIVATE ${LUA_INCLUDE_DIR})

# No liblua on purpose: the module uses the interpreter's own Lua, and a
# second copy linked in crashes as soon as both touch the same state.
target_link_libraries(pcore PRIVATE PitchShifter::Core)
if(APPLE)
    target_link_options(pcore PRIVATE -undefined dynamic_lookup)
endif()

if(WIN32)
    # A DLL can't leave the Lua API undefined, so here it does link Lua: the
    # lua54.dll the interpreter uses. Beside the .exe is where Windows finds DLLs.
    find_library(LUA_LIBRARY NAMES lua5.4 lua54 lua)
    target_link_libraries(pcore PRIVATE ${LUA_LIBRARY})
    set(PANEL_LIB_DIR ${CMAKE_INSTALL_BINDIR})
else()
    set(PANEL_LIB_DIR ${CMAKE_INSTALL_LIBDIR}/pitchshifter)
endif()

install(TARGETS pcore LIBRARY DESTINATION ${PANEL_LIB_DIR})
install(FILES main.lua DESTINATION ${CMAKE_INSTALL_DATADIR}/pitchshifter)
install(DIRECTORY view DESTINATION ${CMAKE_INSTALL_DATADIR}/pitchshifter)
if(IUP_BUNDLE_DIR)
    install(DIRECTORY ${IUP_BUNDLE_DIR}/ DESTINATION ${PANEL_LIB_DIR} USE_SOURCE_PERMISSIONS)
endif()

if(UNIX AND NOT APPLE)
    # The application menu entry and its icon
    install(FILES ${CMAKE_SOURCE_DIR}/pkg/linux/pitchshifter.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
    install(FILES ${CMAKE_SOURCE_DIR}/pkg/linux/pitchshifter.png DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/256x256/apps)
endif()

#############################################################################
## pitchshifter-gui launcher, run target and smoke test
## (on Windows PitchShifter.exe starts the panel itself: there's no sh to run this)

if(LUA_EXECUTABLE AND NOT WIN32)
    # Build tree: the script straight from the sources, the module from build/lib
    set(PCORE_DIR ${CMAKE_LIBRARY_OUTPUT_DIRECTORY})
    set(GUI_SCRIPT ${CMAKE_CURRENT_SOURCE_DIR}/main.lua)
    set(LUA_CANDIDATES "\"${LUA_EXECUTABLE}\"")
    configure_file(pitchshifter-gui.in ${CMAKE_CURRENT_BINARY_DIR}/build-tree/pitchshifter-gui @ONLY)
    file(COPY ${CMAKE_CURRENT_BINARY_DIR}/build-tree/pitchshifter-gui
        DESTINATION ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
        FILE_PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
    )

    # Installed: relative to the launcher ($here), so the release archives unpack anywhere
    file(RELATIVE_PATH PCORE_DIR ${CMAKE_INSTALL_FULL_BINDIR} ${CMAKE_INSTALL_FULL_LIBDIR}/pitchshifter)
    file(RELATIVE_PATH GUI_SCRIPT ${CMAKE_INSTALL_FULL_BINDIR} ${CMAKE_INSTALL_FULL_DATADIR}/pitchshifter/main.lua)
    set(PCORE_DIR "$here/${PCORE_DIR}")
    set(GUI_SCRIPT "$here/${GUI_SCRIPT}")
    # Release archives run on other systems, where a plain "lua" may not be 5.4
    set(LUA_CANDIDATES "lua5.4 \"${LUA_EXECUTABLE}\" /opt/homebrew/opt/lua@5.4/bin/lua lua")
    configure_file(pitchshifter-gui.in ${CMAKE_CURRENT_BINARY_DIR}/install-tree/pitchshifter-gui @ONLY)
    install(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/install-tree/pitchshifter-gui DESTINATION ${CMAKE_INSTALL_BINDIR})

    # In ALL, so every build ends with the panel open. Not on CI, which has no
    # display, and "|| true" because a panel that can't start isn't a failed build.
    if(OPEN_GUI_AFTER_BUILD AND NOT iuplua_missing AND NOT DEFINED ENV{CI})
        set(RUN_GUI_IN_ALL ALL)
    endif()

    # Runs from the repository root, where presets.txt lives
    add_custom_target(run-gui ${RUN_GUI_IN_ALL}
        COMMAND ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/pitchshifter-gui || true
        DEPENDS pcore
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        USES_TERMINAL
    )
    if(TARGET PitchShifter)
        add_dependencies(run-gui PitchShifter)   # a failed compile opens nothing
    endif()
endif()

if(LUA_EXECUTABLE AND BUILD_TESTS)
    # Both save presets.txt where they run, so each gets a disposable directory
    file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/pcore_smoke ${CMAKE_CURRENT_BINARY_DIR}/panel_smoke)
    add_test(NAME pcore_smoke
        COMMAND ${LUA_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/pcore_smoke.lua $<TARGET_FILE_DIR:pcore> --scratch
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/pcore_smoke
    )
    add_test(NAME panel_smoke
        COMMAND ${LUA_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tests/panel_smoke.lua $<TARGET_FILE_DIR:pcore> ${CMAKE_CURRENT_SOURCE_DIR}/main.lua --scratch
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/panel_smoke
    )
endif()
