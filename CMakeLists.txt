cmake_minimum_required(VERSION 3.16)

# When the Raspberry Pi Pico SDK is available (PICO_SDK_PATH set, and a
# pico_sdk_import.cmake placed next to this file per the standard SDK setup), initialise
# it before the project() call so the firmware executable targets can link pico_stdlib.
# Without the SDK this is skipped and only the host libraries/tests are configured.
if (DEFINED ENV{PICO_SDK_PATH} OR PICO_SDK_PATH)
    include(${CMAKE_CURRENT_LIST_DIR}/pico_sdk_import.cmake OPTIONAL)
endif()

project(cansat_2026 LANGUAGES C CXX ASM)

if (COMMAND pico_sdk_init)
    pico_sdk_init()
endif()

enable_testing()

add_subdirectory(firmware/flight-computer)
add_subdirectory(firmware/ground-station)
