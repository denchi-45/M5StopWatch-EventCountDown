#!/bin/sh
# Builds and runs the host unit tests. Needs g++ (C++17) and ArduinoJson,
# which PlatformIO fetches into .pio/libdeps on the first `pio run`.
set -e
cd "$(dirname "$0")/.."
AJ=$(ls -d .pio/libdeps/*/ArduinoJson/src 2>/dev/null | head -n 1)
[ -n "$AJ" ] || { echo "ArduinoJson not found: run 'pio pkg install' first" >&2; exit 2; }
mkdir -p .pio/host-tests
g++ -std=c++17 -O1 -Wall -Wextra -Wno-unused-parameter -I"$AJ" \
    tests/test_main.cpp \
    src/core/CountdownEngine.cpp src/core/EventRepository.cpp src/core/Utf8.cpp \
    src/core/ClockService.cpp src/core/SyncScheduler.cpp src/core/DateEditor.cpp \
    src/input/InputRouter.cpp src/services/JsonCodec.cpp \
    -o .pio/host-tests/run_tests
.pio/host-tests/run_tests
