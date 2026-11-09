#!/bin/sh
#
# Runs a command until it works, for downloads from mirrors that stall.
#
#   pkg/retry.sh pacman --noconfirm -S --needed zip
#
for delay in 15 45 90 180; do
    "$@" && exit 0
    echo "retry.sh: failed, trying again in ${delay}s" >&2
    sleep "$delay"
done
exec "$@"
