cmake_minimum_required(VERSION 3.16)
project(cansat_common LANGUAGES CXX)

add_library(cansat_common STATIC src/telemetry.cpp)
target_include_directories(cansat_common PUBLIC include)
target_compile_features(cansat_common PUBLIC cxx_std_17)
target_compile_options(cansat_common PRIVATE -Wall -Wextra -Wpedantic)

# Host tests. The SX1278 driver reaches hardware only through a callback struct, so its
# whole register sequence is exercised here against a fake register bank.
#
# Skipped when cross-compiling: these link against the host C library and would be built
# for bare-metal ARM in a Pico SDK configure, where they fail at link with missing
# _write/_sbrk/_getpid. The firmware images are the only useful output of that build.
if (NOT CMAKE_CROSSCOMPILING)
    enable_testing()

    add_executable(sx1278_tests tests/sx1278_test.cpp src/sx1278.cpp)
    target_link_libraries(sx1278_tests PRIVATE cansat_common)
    target_compile_options(sx1278_tests PRIVATE -Wall -Wextra -Wpedantic)
    add_test(NAME sx1278_tests COMMAND sx1278_tests)
endif()
