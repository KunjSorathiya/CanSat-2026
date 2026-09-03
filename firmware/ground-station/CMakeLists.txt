cmake_minimum_required(VERSION 3.16)
project(cansat_ground_bridge LANGUAGES CXX)

# Radio-side boundary contract for the PC application.
add_library(ground_radio_bridge INTERFACE)
target_include_directories(ground_radio_bridge INTERFACE include)
target_compile_features(ground_radio_bridge INTERFACE cxx_std_17)

# Host-buildable USB-serial framing (shared logic, mirrored by the Python transport).
add_library(ground_framing STATIC src/framing.cpp)
target_include_directories(ground_framing PUBLIC include)
target_compile_features(ground_framing PUBLIC cxx_std_17)
target_compile_options(ground_framing PRIVATE -Wall -Wextra -Wpedantic)

# Ground-station Pico bridge image (needs the Pico SDK).
if (PICO_SDK_PATH OR DEFINED ENV{PICO_SDK_PATH})
    add_executable(cansat_ground_bridge_firmware
        src/pico/main.cpp
        ../common/src/sx1278.cpp)
    target_include_directories(cansat_ground_bridge_firmware PRIVATE
        include ../common/include)
    target_link_libraries(cansat_ground_bridge_firmware PRIVATE
        ground_framing pico_stdlib hardware_spi hardware_gpio hardware_watchdog)
    target_compile_definitions(cansat_ground_bridge_firmware PRIVATE PICO_BUILD=1)
    target_compile_features(cansat_ground_bridge_firmware PRIVATE cxx_std_17)
    if (COMMAND pico_enable_stdio_usb)
        pico_enable_stdio_usb(cansat_ground_bridge_firmware 1)
        pico_enable_stdio_uart(cansat_ground_bridge_firmware 0)
    endif()
    if (COMMAND pico_add_extra_outputs)
        pico_add_extra_outputs(cansat_ground_bridge_firmware)
    endif()
endif()

enable_testing()
add_executable(ground_framing_test tests/framing_test.cpp)
target_link_libraries(ground_framing_test PRIVATE ground_framing)
target_compile_options(ground_framing_test PRIVATE -Wall -Wextra -Wpedantic)
add_test(NAME ground_framing_test COMMAND ground_framing_test)
