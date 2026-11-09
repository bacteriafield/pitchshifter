#!/bin/sh
#
# Regenerates every icon from docs/static/digitech-vocal300.png. Needs ImageMagick;
# the results are committed, so building doesn't.
#
#   src/gui/view/icon.rgba      the window and taskbar icon, 64x64 raw RGBA (IUP's PNG loader isn't shipped)
#   src/cli/pitchshifter.ico    the icon inside PitchShifter.exe
#   pkg/linux/pitchshifter.png  the application menu's icon
#
set -eu
cd "$(dirname "$0")/.."

src=docs/static/digitech-vocal300.png
square="-trim +repage -background none -gravity center"

mkdir -p pkg/linux
# shellcheck disable=SC2086 # word splitting is the point
convert "$src" $square -resize 64x64 -extent 64x64 -depth 8 RGBA:src/gui/view/icon.rgba
# shellcheck disable=SC2086
convert "$src" $square -resize 256x256 -extent 256x256 pkg/linux/pitchshifter.png
# shellcheck disable=SC2086
convert "$src" $square -resize 256x256 -extent 256x256 -define icon:auto-resize=256,64,48,32,16 src/cli/pitchshifter.ico
