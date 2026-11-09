#!/bin/sh
#
# Puts the runtime the front panel needs into <out>, for cmake -DIUP_BUNDLE_DIR=<out>:
# IUP, which no package manager carries, and on Linux and macOS a private Lua 5.4,
# because IUP's modules only load into 5.4 and a system's Lua can be any version
# (Fedora 45's is 5.5, with no 5.4 package). Windows gets its Lua from MSYS2.
#
#   pkg/fetch-iup.sh linux-x86_64|macos-arm64|windows-x86_64 <out>
#
set -eu

target=$1
mkdir -p "$2"
out=$(cd "$2" && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
cd "$work"

sha=$(command -v sha256sum || echo "shasum -a 256")

# Checksums are pinned because these files end up inside our releases.
fetch() {  # <url, spaces as %20> <sha256>
    file=$(basename "$1")
    # SourceForge mirrors stall a lot: drop a dead transfer and resume where it stopped
    curl -fsSL --retry 8 --retry-all-errors --connect-timeout 30 --speed-limit 2048 --speed-time 60 -C - -o "$file" "$1"
    echo "$2  $file" | $sha -c -
}

iup=https://downloads.sourceforge.net/project/iup/3.32

private_lua() {  # <platform name in Lua's makefile>
    fetch https://www.lua.org/ftp/lua-5.4.8.tar.gz 4f18ddae154e793e46eeab727c59ef1c0c0c2b744e7b94219710d76f530629ae
    tar -xzf lua-5.4.8.tar.gz
    make -C lua-5.4.8 "$1"
    cp lua-5.4.8/src/lua "$out/pitchshifter-lua"
}

case "$target" in
linux-x86_64)
    private_lua linux
    # Linux515 is Ubuntu 22.04, the oldest glibc the release supports
    fetch "$iup/Linux%20Libraries/iup-3.32_Linux515_64_lib.tar.gz" 5fceb30fc3d01d9899302f0df07e590df2326eec0187521e48b5efbe846fff0f
    fetch "$iup/Linux%20Libraries/Lua54/iup-3.32-Lua54_Linux515_64_lib.tar.gz" 6bdfcb2fc230ad7e5da2af4c51ee16a5fab4bdd2ed8447df4842c2be7a2d9b52
    tar -xzf iup-3.32_Linux515_64_lib.tar.gz libiup.so
    tar -xzf iup-3.32-Lua54_Linux515_64_lib.tar.gz libiuplua54.so
    cp libiup.so "$out/"
    cp libiuplua54.so "$out/iuplua.so"          # the name require("iuplua") looks for
    patchelf --set-rpath '$ORIGIN' "$out/iuplua.so"
    ;;

windows-x86_64)
    fetch "$iup/Windows%20Libraries/Dynamic/iup-3.32_Win64_dllw6_lib.zip" 05b00f0406c023ebb14564792c5bba5a7d3c69c8b4a568889d95e8715dac1575
    fetch "$iup/Windows%20Libraries/Dynamic/Lua54/iup-3.32-Lua54_Win64_dllw6_lib.zip" 9eafa8c63ba5d8e3f825d8b8dcc663c80b4aa7315ec4881967695e6de067fa39
    unzip -q iup-3.32_Win64_dllw6_lib.zip iup.dll
    unzip -q iup-3.32-Lua54_Win64_dllw6_lib.zip iuplua54.dll
    cp iup.dll "$out/"
    cp iuplua54.dll "$out/iuplua.dll"
    ;;

macos-arm64)
    # IUP publishes no macOS binaries, so build it. ponytail: compiled directly
    # because its makefiles' macOS GTK rules are stale (GTK2 library names, and
    # GTK_MAC pulls in GTK2 and 32-bit Carbon calls). GDK_NULL is its own "GTK3,
    # no X11" configuration, and the file lists below match src/config.mak for it.
    # Not the Cocoa driver: its text drawing ignores font, size and colour.
    private_lua macosx
    fetch "$iup/Docs%20and%20Sources/iup-3.32_Sources.tar.gz" 871e17fca0fb04d0a13b68d2fdb9883596337826ea44c9254ca38e3eabb6a6a5
    tar -xzf iup-3.32_Sources.tar.gz
    cd iup

    # Old C that newer clang rejects by default
    lenient="-Wno-implicit-function-declaration -Wno-int-conversion -Wno-incompatible-function-pointer-types"

    gtk=$(ls src/gtk/iupgtk_*.c | grep -v iupgtk_draw_gdk)
    # shellcheck disable=SC2086 # word splitting is the point
    cc -O2 -fPIC $lenient -DGDK_NULL -Iinclude -Isrc -Isrc/gtk $(pkg-config --cflags gtk+-3.0) \
        -dynamiclib -install_name @loader_path/libiup.dylib -o "$out/libiup.dylib" \
        src/iup.c src/iup_*.c $gtk src/mot/iupunix_info.c $(pkg-config --libs gtk+-3.0)

    cd srclua5
    mkdir -p lh
    for f in iuplua.lua constants.lua iup_config.lua elem/*.lua; do
        lua bin2c.lua "$f" > "lh/$(basename "${f%.lua}").lh"
    done
    for f in elem/*.lua; do
        lua generator.lua "$f"
    done

    lua_h=$(find "$(brew --prefix lua@5.4)/include" -name lua.h | head -n 1)
    # shellcheck disable=SC2086
    cc -O2 -fPIC $lenient -DIUPLUA_USELH -I../include -I../src -I. -Ilh -I"$(dirname "$lua_h")" \
        -bundle -undefined dynamic_lookup -o "$out/iuplua.so" \
        iuplua.c iuplua_api.c iuplua_draw.c iuplua_tree_aux.c iuplua_scanf.c \
        iuplua_getparam.c iuplua_getcolor.c iuplua_config.c elem/il_*.c \
        -L"$out" -liup
    ;;

*)
    echo "fetch-iup.sh: unknown target '$target'" >&2
    exit 1
    ;;
esac
