#!/usr/bin/env bash
#
# PitchShifter installer for Linux and macOS (Windows: install.ps1).
# Installs the runtime dependencies and the release build from GitHub.
#
#   curl -fsSL https://raw.githubusercontent.com/bacteriafield/pitchshifter/main/install.sh | bash
#   curl -fsSL ... | PREFIX=$HOME/.local VERSION=v0.0.1 bash
#
set -euo pipefail

REPO="bacteriafield/pitchshifter"
PREFIX="${PREFIX:-/usr/local}"
VERSION="${VERSION:-latest}"
SUDO=""
[ "$(id -u)" -ne 0 ] && SUDO="sudo"

have() { command -v "$1" >/dev/null 2>&1; }

echo "==> Installing dependencies (PortAudio, Lua 5.4)"
case "$(uname -s)-$(uname -m)" in
Linux-x86_64)
    target=linux-x86_64
    if have apt-get; then
        $SUDO apt-get update
        $SUDO apt-get install -y curl libportaudio2 lua5.4
    elif have dnf; then
        $SUDO dnf install -y curl portaudio lua
    elif have pacman; then
        $SUDO pacman -Sy --needed --noconfirm curl portaudio lua54
    elif have zypper; then
        $SUDO zypper install -y curl libportaudio2 lua54
    else
        echo "Unknown package manager: install PortAudio and Lua 5.4 yourself." >&2
    fi
    ;;
Darwin-arm64)
    target=macos-arm64
    have brew || { echo "Homebrew is required: https://brew.sh" >&2; exit 1; }
    brew install portaudio lua@5.4
    ;;
*)
    echo "No release build for $(uname -s) $(uname -m). Build from source, see the README." >&2
    exit 1
    ;;
esac

if [ "$VERSION" = latest ]; then
    url="https://github.com/$REPO/releases/latest/download/pitchshifter-$target.tar.gz"
else
    url="https://github.com/$REPO/releases/download/$VERSION/pitchshifter-$target.tar.gz"
fi
echo "==> Downloading $url"
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
curl -fL "$url" | tar -xz -C "$tmp" --strip-components=1

echo "==> Installing to $PREFIX"
mkdir -p "$PREFIX" 2>/dev/null || $SUDO mkdir -p "$PREFIX"
if [ -w "$PREFIX" ]; then
    cp -R "$tmp"/. "$PREFIX"/
else
    $SUDO cp -R "$tmp"/. "$PREFIX"/
fi

echo
echo "Done. Run: PitchShifter"
# ponytail: IUP isn't in apt/dnf/pacman/brew, so it stays a manual step
if [ -x "$PREFIX/bin/pitchshifter-gui" ]; then
    # same lookup as the launcher
    for lua in lua5.4 /opt/homebrew/opt/lua@5.4/bin/lua lua; do have "$lua" && break; done
    if "$lua" -e 'os.exit(package.searchpath("iuplua", package.cpath) and 0 or 1)' 2>/dev/null; then
        echo "     or:  pitchshifter-gui"
    else
        echo "The front panel (pitchshifter-gui) also needs IUP for Lua 5.4 (iuplua):"
        echo "  https://sourceforge.net/projects/iup/files/"
    fi
fi
case ":$PATH:" in *":$PREFIX/bin:"*) ;; *) echo "Add $PREFIX/bin to your PATH." ;; esac
