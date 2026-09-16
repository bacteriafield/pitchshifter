#!/bin/sh
# Fills the package manifests in packaging/ with a release's version and checksums.
#
#   packaging/render.sh 0.0.1 dist/SHA256SUMS out/
#
set -eu
version=$1 sums=$2 out=$3

sha() {
    s=$(awk -v f="$1" '$2 == f || $2 == "*" f {print $1}' "$sums")
    [ -n "$s" ] || { echo "render.sh: no checksum for $1 in $sums" >&2; exit 1; }
    echo "$s"
}
linux=$(sha pitchshifter-linux-x86_64.tar.gz)
macos=$(sha pitchshifter-macos-arm64.tar.gz)
windows=$(sha pitchshifter-windows-x86_64.zip)

mkdir -p "$out"
cp -R "$(dirname "$0")"/homebrew "$(dirname "$0")"/aur "$(dirname "$0")"/scoop "$(dirname "$0")"/chocolatey "$out"/
find "$out" -type f -exec sed -i \
    -e "s/@VERSION@/$version/g" \
    -e "s/@SHA256_LINUX@/$linux/g" \
    -e "s/@SHA256_MACOS@/$macos/g" \
    -e "s/@SHA256_WINDOWS@/$windows/g" {} +

if grep -rn '@VERSION@\|@SHA256_' "$out"; then
    echo "render.sh: unfilled placeholders above" >&2
    exit 1
fi
