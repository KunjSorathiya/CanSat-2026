cmake_minimum_required(VERSION 3.16)
project(cansat_flight LANGUAGES CXX)

add_subdirectory(../common common_build)

add_library(flight_core STATIC src/controller.cpp src/state_machine.cpp src/gps_parser.cpp src/fault_manager.cpp)
target_include_directories(flight_core PUBLIC include)
target_link_libraries(flight_core PUBLIC cansat_common)
target_compile_features(flight_core PUBLIC cxx_std_17)
target_compile_options(flight_core PRIVATE -Wall -Wextra -Wpedantic)

add_executable(flight_smoke_test tests/flight_smoke_test.cpp)
target_include_directories(flight_smoke_test PRIVATE tests)
target_link_libraries(flight_smoke_test PRIVATE flight_core)
target_compile_options(flight_smoke_test PRIVATE -Wall -Wextra -Wpedantic)
enable_testing()
add_test(NAME flight_smoke_test COMMAND flight_smoke_test)

add_executable(flight_demo tests/flight_smoke_test.cpp)
target_include_directories(flight_demo PRIVATE tests)
target_link_libraries(flight_demo PRIVATE flight_core)
