#!/usr/bin/env bash
# Compila o Godot 2.1 para 3DS (e opcionalmente o editor Linux) dentro do container devkitPro.
# Uso (a partir de qualquer pasta): misc/3ds-build/build.sh [template|editor|package|all]
#   template : compila bin/godot.3ds.elf, gera bin/godot.3dsx (sem jogo) e o instala como template
#              do editor em ~/.godot/templates/3ds_release.3dsx (INSTALL_TEMPLATE=0 para não instalar)
#              OPTIMIZE=max|speed|size (-O3/-O2/-Os; padrão speed)
#   package  : embute um projeto na RomFS -> bin/<nome>.3dsx
#              PROJECT=<pasta do projeto, relativa à raiz do repo> (padrão: misc/3ds-build/test_project)
#   WITH_EDITOR=0 misc/3ds-build/build.sh template   -> imagem sem libs X11 (só template 3DS)
#   editor: compila no Debian do container; para rodar no WSL/Ubuntu prefira compilar nativo
#           (veja COMPILAR.md), pois o binário pode exigir uma glibc mais nova que a do host.
set -euo pipefail
HERE=$(cd "$(dirname "$0")" && pwd)
SRC=$(cd "$HERE/../.." && pwd)
WHAT=${1:-template}
IMG=godot-3ds-build
JOBS=$(nproc)

docker build --build-arg WITH_EDITOR="${WITH_EDITOR:-1}" -t "$IMG" "$HERE"
run() { docker run --rm -v "$SRC":/src "$IMG" bash -c "$1"; }

if [[ $WHAT == template || $WHAT == all ]]; then
  run "scons -j$JOBS platform=3ds target=release tools=no disable_3d=yes optimize=${OPTIMIZE:-speed} \
    && arm-none-eabi-size bin/godot.3ds.elf \
    && smdhtool --create 'Godot 2.1 3DS' 'Template' 'Homebrodot' misc/3ds-build/smdh_icon.png bin/godot.smdh \
    && 3dsxtool bin/godot.3ds.elf bin/godot.3dsx --smdh=bin/godot.smdh"
  # Install as the editor's export template (Exportar -> Nintendo 3DS).
  if [[ "${INSTALL_TEMPLATE:-1}" == 1 ]]; then
    mkdir -p "$HOME/.godot/templates"
    cp "$SRC/bin/godot.3dsx" "$HOME/.godot/templates/3ds_release.3dsx"
    echo "Template instalado em $HOME/.godot/templates/3ds_release.3dsx"
  fi
fi
if [[ $WHAT == package || $WHAT == all ]]; then
  PROJECT=${PROJECT:-misc/3ds-build/test_project}
  NAME=$(basename "$PROJECT")
  [[ -f "$SRC/bin/godot.3ds.elf" ]] || { echo "Compile o template primeiro (build.sh template)"; exit 1; }
  run "smdhtool --create '$NAME' 'Godot 2.1 3DS' 'RetroMoi' misc/3ds-build/smdh_icon.png bin/$NAME.smdh \
    && 3dsxtool bin/godot.3ds.elf bin/$NAME.3dsx --smdh=bin/$NAME.smdh --romfs='$PROJECT'"
fi
if [[ $WHAT == editor || $WHAT == all ]]; then
  run "scons -j$JOBS platform=x11 tools=yes target=release_debug"
fi
ls -la "$SRC/bin"
