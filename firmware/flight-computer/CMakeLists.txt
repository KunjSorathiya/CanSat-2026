cmake_minimum_required(VERSION 3.16)
project(cansat_flight LANGUAGES CXX)

add_subdirectory(../common common_build)

# Hardware-independent flight core. Builds on the host toolchain and on the Pico.
set(FLIGHT_CORE_SOURCES
    src/config.cpp
    src/controller.cpp
    src/state_machine.cpp
    src/gps_parser.cpp
    src/fault_manager.cpp
    src/orientation.cpp
    src/scheduler.cpp
    src/sensor_math.cpp
    src/telemetry_builder.cpp
    src/raw_block_log.cpp
    src/startup_calibration.cpp
    src/health.cpp)

add_library(flight_core STATIC ${FLIGHT_CORE_SOURCES})
target_include_directories(flight_core PUBLIC include)
target_link_libraries(flight_core PUBLIC cansat_common)
target_compile_features(flight_core PUBLIC cxx_std_17)
target_compile_options(flight_core PRIVATE -Wall -Wextra -Wpedantic)

# Pico firmware image. Only configured when the Pico SDK is available; the hardware
# drivers live under src/pico/ and are never compiled into host builds.
if (PICO_SDK_PATH OR DEFINED ENV{PICO_SDK_PATH})
    add_executable(cansat_pico_firmware
        src/pico/main.cpp
        src/pico/pico_hal.cpp
        src/pico/mpu6050.cpp
        src/pico/bmp280.cpp
        src/pico/neo6m.cpp
        src/pico/sd_card.cpp
        src/pico/sd_logger.cpp
        src/pico/pico_radio.cpp
        ../common/src/sx1278.cpp)
    target_include_directories(cansat_pico_firmware PRIVATE
        include ../common/include)
    target_link_libraries(cansat_pico_firmware PRIVATE
        flight_core
        pico_stdlib
        hardware_i2c
        hardware_spi
        hardware_uart
        hardware_adc
        hardware_gpio
        hardware_watchdog)
    target_compile_definitions(cansat_pico_firmware PRIVATE PICO_BUILD=1)
    target_compile_features(cansat_pico_firmware PRIVATE cxx_std_17)
    if (COMMAND pico_enable_stdio_usb)
        pico_enable_stdio_usb(cansat_pico_firmware 1)
        pico_enable_stdio_uart(cansat_pico_firmware 0)
    endif()
    if (COMMAND pico_add_extra_outputs)
        pico_add_extra_outputs(cansat_pico_firmware)
    endif()
endif()

# Host tests.
enable_testing()

add_executable(flight_smoke_test tests/flight_smoke_test.cpp)
target_include_directories(flight_smoke_test PRIVATE tests)
target_link_libraries(flight_smoke_test PRIVATE flight_core)
target_compile_options(flight_smoke_test PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME flight_smoke_test COMMAND flight_smoke_test)

add_executable(flight_tests tests/flight_tests.cpp)
target_include_directories(flight_tests PRIVATE tests)
target_link_libraries(flight_tests PRIVATE flight_core)
target_compile_options(flight_tests PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME flight_tests COMMAND flight_tests)
