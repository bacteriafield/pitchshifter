#!/usr/bin/env bash
#
#    MIT License
#
#    Copyright (c) 2025 Evandro
#
#    Permission is hereby granted, free of charge, to any person obtaining a copy
#    of this software and associated documentation files (the "Software"), to deal
#    in the Software without restriction, including without limitation the rights
#    to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
#    copies of the Software, and to permit persons to whom the Software is
#    furnished to do so, subject to the following conditions:
#
#    The above copyright notice and this permission notice shall be included in all
#    copies or substantial portions of the Software.
#
#    THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
#    IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
#    FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
#    AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
#    LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
#    OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
#    SOFTWARE.
#

set -e

mkdir -p build && cd build
cmake ..
cmake --build . -j$(nproc)

UNAME=$(uname -s 2>/dev/null || echo Unknown)

have_cmd() { command -v "$1" >/dev/null 2>&1; }

install_linux_deb() {
  sudo apt update
  sudo apt install -y alsa-utils
  sudo apt install -y portaudio19-dev
  sudo apt install pulseaudio -y
}

install_linux_fedora() {
  sudo dnf install -y alsa-utils
  sudo dnf install -y pulseaudio
}

install_linux_arch() {
  sudo pacman -Sy --noconfirm alsa-utils
  sudo pacman -Sy --noconfirm portaudio
}

install_linux_opensuse() {
  sudo zypper install -y alsa-utils
  sudo zypper install -y portaudio
}

install_darwin() {
  brew install portaudio
  brew install switchaudio-osx
}

detect_pkg_manager_and_install() {
  if have_cmd apt; then
    install_linux_deb
  elif have_cmd apt-get; then
    install_linux_deb
  elif have_cmd dnf; then
    install_linux_fedora
  elif have_cmd yum; then
    install_linux_fedora
  elif have_cmd pacman; then
    install_linux_arch
  elif have_cmd zypper; then
    install_linux_opensuse
  elif have_cmd brew; then
    install_darwin
  else
    echo "Unsupported package manager"
    exit 1
  fi
}

case ""$UNAME"" in
Linux)
    if have_cmd arecord && have_cmd aplay; then
        echo "Already installed"
    else
        echo "Not found. Installing..."
        detect_pkg_manager_and_install
    fi

    if have_cmd arecord && have_cmd aplay; then
      arecord -l || true
      aplay -l || true
    else
      echo "Failt to install arecord/aplay. Install manually 'alsa-utils'."
      exit 1
    fi
    ;;
    
Darwin)
    if have_cmd arecord && have_cmd aplay; then
        echo "Already installed"
    else
        echo "Not found. Installing..."
        install_darwin
    fi
    ;;

*)
  echo "Unsupported OS"
  exit 1;;
esac