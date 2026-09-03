cmake_minimum_required(VERSION 3.16)
project(cansat_common LANGUAGES CXX)

add_library(cansat_common STATIC src/telemetry.cpp)
target_include_directories(cansat_common PUBLIC include)
target_compile_features(cansat_common PUBLIC cxx_std_17)
target_compile_options(cansat_common PRIVATE -Wall -Wextra -Wpedantic)
