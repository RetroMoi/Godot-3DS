#!/usr/bin/env bash
# Packs a Godot 2.1 project with the current 3DS template and runs it in the Azahar libretro
# core (headless, software renderer).
#   run3ds.sh <project dir> <frames> <out dir> [script]   (env LR_OLD3DS=1: Old 3DS)
# Screenshots: <out dir>/frame_NNNNN.ppm (+ .png). See lr_harness.c for the script format.
set -euo pipefail
PROJ=$(realpath "$1"); FRAMES=$2; OUT=$3; SCRIPT=${4:-}
HERE=$(cd "$(dirname "$0")" && pwd)
GODOT=/home/user/godot-2.1
mkdir -p "$OUT"
NAME=$(basename "$PROJ")
rm -rf "$GODOT/misc/3ds-build/_run3ds"
cp -r "$PROJ" "$GODOT/misc/3ds-build/_run3ds"
docker info >/dev/null 2>&1 || { setsid nohup dockerd >/tmp/dockerd.log 2>&1 < /dev/null & disown; for i in $(seq 1 20); do docker info >/dev/null 2>&1 && break; sleep 1; done; }
docker run --rm -v "$GODOT":/src -w /src godot-3ds-build bash -c \
  "smdhtool --create '$NAME' 'test' 'test' misc/3ds-build/smdh_icon.png bin/_run3ds.smdh >/dev/null && 3dsxtool bin/godot.3ds.elf bin/_run3ds.3dsx --smdh=bin/_run3ds.smdh --romfs=misc/3ds-build/_run3ds >/dev/null"
rm -rf "$GODOT/misc/3ds-build/_run3ds"
# DSP "firmware" for ndspInit: the emulator's HLE DSP ignores the contents.
mkdir -p "$OUT/system/Azahar/sdmc/3ds"
[ -f "$OUT/system/Azahar/sdmc/3ds/dspfirm.cdc" ] || head -c 4096 /dev/zero > "$OUT/system/Azahar/sdmc/3ds/dspfirm.cdc"
"$HERE/lr_harness" "$HERE/azahar_libretro.so" "$GODOT/bin/_run3ds.3dsx" "$FRAMES" "$OUT" $SCRIPT 2>&1 | grep -v "GetPhysMemRegionInfo" || true
for f in "$OUT"/frame_*.ppm; do [ -e "$f" ] && convert "$f" "${f%.ppm}.png" && rm "$f"; done
