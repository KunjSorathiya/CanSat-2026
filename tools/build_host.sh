#!/usr/bin/env bash
# Host build + test for the CanSat software that does not require the Pico SDK.
# Compiles the shared telemetry library, the flight core, the ground-station framing
# library and every host test with g++, then runs them. Also runs the Python
# ground-station test suite when python is available.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT="$ROOT/build/host"
mkdir -p "$OUT"

CXX="${CXX:-g++}"
CXXFLAGS="${CXXFLAGS:--std=c++17 -O2 -Wall -Wextra -Wpedantic}"

INC=(
  -I"$ROOT/firmware/common/include"
  -I"$ROOT/firmware/flight-computer/include"
  -I"$ROOT/firmware/flight-computer/tests"
  -I"$ROOT/firmware/ground-station/include"
)

COMMON_SRC=(
  "$ROOT/firmware/common/src/telemetry.cpp"
)
FLIGHT_CORE_SRC=(
  "$ROOT/firmware/flight-computer/src/config.cpp"
  "$ROOT/firmware/flight-computer/src/controller.cpp"
  "$ROOT/firmware/flight-computer/src/state_machine.cpp"
  "$ROOT/firmware/flight-computer/src/gps_parser.cpp"
  "$ROOT/firmware/flight-computer/src/fault_manager.cpp"
  "$ROOT/firmware/flight-computer/src/orientation.cpp"
  "$ROOT/firmware/flight-computer/src/scheduler.cpp"
  "$ROOT/firmware/flight-computer/src/sensor_math.cpp"
  "$ROOT/firmware/flight-computer/src/telemetry_builder.cpp"
  "$ROOT/firmware/flight-computer/src/raw_block_log.cpp"
  "$ROOT/firmware/flight-computer/src/startup_calibration.cpp"
  "$ROOT/firmware/flight-computer/src/health.cpp"
)
GROUND_SRC=(
  "$ROOT/firmware/ground-station/src/framing.cpp"
)

echo "== compiling flight_smoke_test =="
# shellcheck disable=SC2068
$CXX $CXXFLAGS ${INC[@]} \
  ${COMMON_SRC[@]} ${FLIGHT_CORE_SRC[@]} \
  "$ROOT/firmware/flight-computer/tests/flight_smoke_test.cpp" \
  -o "$OUT/flight_smoke_test"

echo "== compiling flight_tests =="
# shellcheck disable=SC2068
$CXX $CXXFLAGS ${INC[@]} \
  ${COMMON_SRC[@]} ${FLIGHT_CORE_SRC[@]} \
  "$ROOT/firmware/flight-computer/tests/flight_tests.cpp" \
  -o "$OUT/flight_tests"

echo "== compiling sx1278_tests =="
# The LoRa driver reaches hardware only through a callback struct, so its whole register
# sequence runs here against a fake register bank -- no radio, no SDK.
# shellcheck disable=SC2068
$CXX $CXXFLAGS ${INC[@]}   "$ROOT/firmware/common/src/sx1278.cpp"   "$ROOT/firmware/common/tests/sx1278_test.cpp"   -o "$OUT/sx1278_tests"

echo "== compiling sd_card_tests =="
# The microSD driver also reaches hardware through a callback struct, so its command
# sequence runs against a simulated card built from the SD SPI-mode specification.
# shellcheck disable=SC2068
$CXX $CXXFLAGS ${INC[@]}   "$ROOT/firmware/flight-computer/src/pico/sd_card.cpp"   "$ROOT/firmware/flight-computer/tests/sd_card_test.cpp"   -o "$OUT/sd_card_tests"

echo "== compiling ground_station_tests =="
# shellcheck disable=SC2068
$CXX $CXXFLAGS ${INC[@]} \
  ${COMMON_SRC[@]} ${GROUND_SRC[@]} \
  "$ROOT/firmware/ground-station/tests/framing_test.cpp" \
  -o "$OUT/ground_station_tests"

echo "== running C++ tests =="
"$OUT/flight_smoke_test"
"$OUT/flight_tests" "$ROOT"
"$OUT/sx1278_tests"
"$OUT/sd_card_tests"
"$OUT/ground_station_tests"

if command -v python >/dev/null 2>&1; then
  echo "== running Python ground-station tests =="
  ( cd "$ROOT" && python -m unittest discover -s ground-station/software/tests -p "test_*.py" -v )

  echo "== running Python tooling tests =="
  ( cd "$ROOT" && python -m unittest discover -s tools/tests -p "test_*.py" )
fi

# The web console is a single self-contained HTML file with no build step. Its parser,
# validator, framing and link-health logic are hand-ports, so they are extracted and tested
# under Node when Node is available.
if command -v node >/dev/null 2>&1; then
  echo "== running web console tests =="
  ( cd "$ROOT" && node --test ground-station/web/tests/console_core.test.mjs )
else
  echo "== SKIPPED web console tests (node not found) =="
fi

echo "ALL HOST BUILDS AND TESTS PASSED"
