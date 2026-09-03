#!/usr/bin/env bash
# Syntax-check the firmware PICO_BUILD branches against minimal SDK stubs, on a host
# without the real Pico SDK. This is a compile-only smoke check, not a firmware build.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
STUBS="$ROOT/tools/pico_sdk_stubs"
CXX="${CXX:-g++}"

INC=(
  -I"$STUBS"
  -I"$ROOT/firmware/common/include"
  -I"$ROOT/firmware/flight-computer/include"
  -I"$ROOT/firmware/ground-station/include"
)

FILES=(
  firmware/flight-computer/src/pico/main.cpp
  firmware/flight-computer/src/pico/pico_hal.cpp
  firmware/flight-computer/src/pico/pico_radio.cpp
  firmware/flight-computer/src/pico/mpu6050.cpp
  firmware/flight-computer/src/pico/bmp280.cpp
  firmware/flight-computer/src/pico/neo6m.cpp
  firmware/flight-computer/src/pico/sd_card.cpp
  firmware/flight-computer/src/pico/sd_logger.cpp
  firmware/common/src/sx1278.cpp
  firmware/ground-station/src/pico/main.cpp
)

rc=0
for f in "${FILES[@]}"; do
  if [ ! -f "$ROOT/$f" ]; then
    echo "SKIP  $f (missing)"
    continue
  fi
  if $CXX -std=c++17 -fsyntax-only -Wall -Wextra -DPICO_BUILD "${INC[@]}" "$ROOT/$f" 2> >(sed "s/^/  /" >&2); then
    echo "OK    $f"
  else
    echo "FAIL  $f"
    rc=1
  fi
done
exit $rc
