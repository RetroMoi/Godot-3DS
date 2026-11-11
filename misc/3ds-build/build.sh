#!/usr/bin/env bash
# Compila o Godot 2.1 para 3DS (e opcionalmente o editor Linux) dentro do container devkitPro.
# Uso (a partir de qualquer pasta): misc/3ds-build/build.sh [template|editor|all]
#   WITH_EDITOR=0 misc/3ds-build/build.sh template   -> imagem sem libs X11 (só template 3DS)
set -euo pipefail
HERE=$(cd "$(dirname "$0")" && pwd)
SRC=$(cd "$HERE/../.." && pwd)
WHAT=${1:-template}
IMG=godot-3ds-build
JOBS=$(nproc)

docker build --build-arg WITH_EDITOR="${WITH_EDITOR:-1}" -t "$IMG" "$HERE"
run() { docker run --rm -v "$SRC":/src "$IMG" bash -c "$1"; }

if [[ $WHAT == template || $WHAT == all ]]; then
  run "scons -j$JOBS platform=3ds target=release tools=no disable_3d=yes \
    && arm-none-eabi-size bin/godot.3ds.elf \
    && smdhtool --create 'Godot 2.1 3DS' 'Template' 'Homebrodot' \$DEVKITPRO/libctru/default_icon.png bin/godot.smdh \
    && 3dsxtool bin/godot.3ds.elf bin/godot.3dsx --smdh=bin/godot.smdh"
fi
if [[ $WHAT == editor || $WHAT == all ]]; then
  run "scons -j$JOBS platform=x11 tools=yes target=release_debug"
fi
ls -la "$SRC/bin"
