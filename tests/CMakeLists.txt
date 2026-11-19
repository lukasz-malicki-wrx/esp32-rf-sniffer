cmake_minimum_required(VERSION 3.20)
project(rf_bridge_tests LANGUAGES CXX)

# Host-side unit tests for the hardware-independent core of components/rf_bridge.
# The ESPHome glue (rf_bridge.cpp, entities) is exercised by `esphome compile` in CI.

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(RF_BRIDGE_SANITIZE "Build tests with AddressSanitizer + UBSan" ON)
option(RF_BRIDGE_COVERAGE "Instrument the core for line/branch coverage (gcov format)" OFF)

include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.tar.gz
  DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

set(COMPONENT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../components/rf_bridge)

add_executable(rf_bridge_tests
  ${COMPONENT_DIR}/rf_frame.cpp
  test_bounded_queue.cpp
  test_frame_deduplicator.cpp
  test_duty_cycle_limiter.cpp
  test_rf_frame.cpp
  test_tx_scheduler.cpp
)
target_include_directories(rf_bridge_tests PRIVATE ${COMPONENT_DIR})
target_compile_options(rf_bridge_tests PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Werror)
target_link_libraries(rf_bridge_tests PRIVATE GTest::gtest_main)

if(RF_BRIDGE_SANITIZE)
  target_compile_options(rf_bridge_tests PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all)
  target_link_options(rf_bridge_tests PRIVATE -fsanitize=address,undefined)
endif()

if(RF_BRIDGE_COVERAGE)
  target_compile_options(rf_bridge_tests PRIVATE --coverage -O0 -fno-inline)
  target_link_options(rf_bridge_tests PRIVATE --coverage)
endif()

enable_testing()
include(GoogleTest)
gtest_discover_tests(rf_bridge_tests)
