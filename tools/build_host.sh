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
# The extra warnings beyond -Wall -Wextra are the ones that catch embedded mistakes:
# -Wshadow (a local hiding a member), -Wcast-align (a pointer cast that faults on ARM but
# not on x86), -Wdouble-promotion (a float silently widened on a chip with no double FPU),
# -Wformat=2 (a printf format that does not match its argument). The tree is clean under
# all of them; keep it that way.
CXXFLAGS="${CXXFLAGS:--std=c++17 -O2 -Wall -Wextra -Wpedantic -Wshadow -Wnon-virtual-dtor -Wcast-align -Wnull-dereference -Wdouble-promotion -Wformat=2}"

INC=(
  -I"$ROOT/firmware/common/include"
  -I"$ROOT/firmware/flight-computer/include"
  -I"$ROOT/firmware/flight-computer/tests"
  -I"$ROOT/firmware/ground-station/include"
)

COMMON_SRC=(
  "$ROOT/firmware/common/src/telemetry.cpp"
  "$ROOT/firmware/common/src/command.cpp"
)
FLIGHT_CORE_SRC=(
  "$ROOT/firmware/flight-computer/src/config.cpp"
  "$ROOT/firmware/flight-computer/src/fat_volume.cpp"
  "$ROOT/firmware/flight-computer/src/sound_level.cpp"
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

echo "== compiling emit_mission =="
# The vehicle half of the end-to-end test: runs the real controller through a scripted
# mission and prints the packets it transmits, for the Python pipeline to consume.
# shellcheck disable=SC2068
$CXX $CXXFLAGS ${INC[@]}   ${COMMON_SRC[@]} ${FLIGHT_CORE_SRC[@]}   "$ROOT/firmware/flight-computer/tests/emit_mission.cpp"   -o "$OUT/emit_mission"

echo "== compiling sx1278_tests =="
# The LoRa driver reaches hardware only through a callback struct, so its whole register
# sequence runs here against a fake register bank -- no radio, no SDK.
# shellcheck disable=SC2068
$CXX $CXXFLAGS ${INC[@]}   "$ROOT/firmware/common/src/sx1278.cpp"   "$ROOT/firmware/common/tests/sx1278_test.cpp"   -o "$OUT/sx1278_tests"

echo "== compiling fat_volume_tests =="
$CXX $CXXFLAGS ${INC[@]}   "$ROOT/firmware/flight-computer/src/fat_volume.cpp"   "$ROOT/firmware/flight-computer/tests/fat_volume_test.cpp"   -o "$OUT/fat_volume_tests"

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

# Every suite's own count of what it ran is written here as well as to the console, so
# tools/check_doc_claims.py can hold the documented figures to what the suites report.
# Without this the counts in the test plan and the README drift the moment a test is added.
LOG="$OUT/test-output.log"
: > "$LOG"
log() { "$@" 2>&1 | tee -a "$LOG"; }

echo "== running C++ tests =="
log "$OUT/flight_smoke_test"
log "$OUT/flight_tests" "$ROOT"
log "$OUT/sx1278_tests"
log "$OUT/fat_volume_tests"
log "$OUT/sd_card_tests"
log "$OUT/ground_station_tests" "$ROOT"

HAVE_PYTHON=0
if command -v python >/dev/null 2>&1; then
  HAVE_PYTHON=1
  echo "== running Python ground-station tests =="
  ( cd "$ROOT" && log python -m unittest discover -s ground-station/software/tests -p "test_*.py" -v )

  echo "== running Python tooling tests =="
  ( cd "$ROOT" && log python -m unittest discover -s tools/tests -p "test_*.py" )

  # The descent model sizes the parachute and predicts the descent. It is pinned to
  # closed-form limits that can be checked by hand, because the mechanical build takes a
  # canopy diameter from it and there is no second source for that number.
  #
  # Order matters here: check_doc_claims.py reads the "Ran N tests" lines this log
  # accumulates positionally -- ground station, tooling, simulations -- so a new discovery
  # run goes at the END of this block, never between the two above it.
  echo "== running simulation tests =="
  ( cd "$ROOT" && log python -m unittest discover -s simulations/tests -p "test_*.py" )

  # The post-flight analysis runs once for real, inside the rulebook's four-hour window, on
  # data nobody has seen. It is held here to a synthetic flight with known answers, and its
  # notebook is executed cell by cell. Fourth, and last, for the positional reason above.
  # It needs numpy and matplotlib; without them it is skipped rather than failed.
  if python -c "import numpy, matplotlib" >/dev/null 2>&1; then
    echo "== running post-flight analysis tests =="
    ( cd "$ROOT" && MPLBACKEND=Agg log python -m unittest discover -s analysis/tests -p "test_*.py" )
  else
    echo "== SKIPPED post-flight analysis tests (numpy or matplotlib not installed) =="
  fi
fi

# The web console is a single self-contained HTML file with no build step. Its parser,
# validator, framing and link-health logic are hand-ports, so they are extracted and tested
# under Node when Node is available.
if command -v node >/dev/null 2>&1; then
  echo "== running web console tests =="
  ( cd "$ROOT" && log node --test ground-station/web/tests/console_core.test.mjs )
else
  echo "== SKIPPED web console tests (node not found) =="
fi

# Documentation drifts silently: a constant changes and the prose quoting it does not.
# A wrong pin number or telemetry rate in a document is a defect like any other. This runs
# last because it also reads the log every suite above just wrote, and holds the documented
# test counts to the counts the suites actually reported on this run.
if [ "$HAVE_PYTHON" -eq 1 ]; then
  echo "== checking documented claims against the source =="
  ( cd "$ROOT" && python tools/check_doc_claims.py | tail -1 )
fi

echo "ALL HOST BUILDS AND TESTS PASSED"
