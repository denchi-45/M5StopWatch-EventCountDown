#!/bin/sh
# Renders the device screens to PNG on a Linux/WSL host (no hardware needed).
# Needs g++/gcc and the M5GFX sources PlatformIO fetched into .pio/libdeps.
# Output: .pio/preview/out/*.png
set -e
cd "$(dirname "$0")/../.."
ROOT=$(pwd)
G=$(ls -d "$ROOT"/.pio/libdeps/*/M5GFX/src | head -n 1)
B="$ROOT/.pio/preview"
mkdir -p "$B/lib" "$B/out"
DEFS="-DLGFX_LINUX_FB"
# M5GFX (sprite rendering only) - compiled once.
for f in "$G"/lgfx/v1/*.cpp "$G"/lgfx/v1/misc/*.cpp "$G"/lgfx/v1/platforms/framebuffer/*.cpp \
         "$G"/lgfx/v1/panel/Panel_Device.cpp; do
    o="$B/lib/$(echo "$f" | sed "s#.*/src/##; s#/#_#g").o"
    [ -f "$o" ] || g++ -std=c++17 -O1 -w -c $DEFS -I"$G" "$f" -o "$o"
done
for f in "$G"/lgfx/utility/*.c $(find "$G"/lgfx/Fonts -name '*.c'); do
    o="$B/lib/c_$(basename "$f").o"
    [ -f "$o" ] || gcc -O0 -w -c -I"$G" "$f" -o "$o"
done
SRC="src/core/CountdownEngine.cpp src/core/EventRepository.cpp src/core/Utf8.cpp src/core/ClockService.cpp
     src/core/SyncScheduler.cpp src/core/DateEditor.cpp src/input/InputRouter.cpp
     src/ui/Painter.cpp src/ui/HudFont.cpp src/ui/HudWidgets.cpp src/ui/SevenSegmentRenderer.cpp
     src/ui/ProgressRing.cpp src/ui/LayoutPolicy.cpp src/ui/TextUtil.cpp src/ui/CountdownScreen.cpp
     src/ui/SettingsScreen.cpp"
# private -> public only so the harness can load fixture state.
g++ -std=c++17 -O1 -g ${SAN:-} -w $DEFS -Dprivate=public -Dprotected=public -I"$G" -Itools/preview/stubs \
    tools/preview/preview.cpp $SRC "$B"/lib/*.o -o "$B/preview" -lpthread
"$B/preview" "$B/out"
