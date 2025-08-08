cmake_minimum_required(VERSION 3.20)
project(MyCustomBTCMiner LANGUAGES CXX)

option(ENABLE_CUDA "Build CUDA components" OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

# GoogleTest
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# nlohmann/json
FetchContent_Declare(
  nlohmann_json
  URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
)
FetchContent_MakeAvailable(nlohmann_json)

add_subdirectory(src)

enable_testing()

add_executable(unit_tests
  tests/test_endianness.cpp
  tests/test_targets.cpp
  tests/test_registry.cpp
  tests/test_log.cpp
  tests/test_headers_build.cpp
  tests/test_metrics_and_cpuverify.cpp
)
target_link_libraries(unit_tests
  PRIVATE
    miner_core
    GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(unit_tests)


