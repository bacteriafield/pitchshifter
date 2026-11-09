#!/bin/sh
#
# Does the real window open? A panel that can't start falls back to the prompt,
# which exits on empty stdin, so "still running" means it really opened.
#
#   tests/panel_opens.sh [xvfb-run -a] path/to/PitchShifter
#
set -eu
"$@" </dev/null &
pid=$!
sleep 10
kill -0 "$pid"
kill "$pid"
echo "panel opened"
