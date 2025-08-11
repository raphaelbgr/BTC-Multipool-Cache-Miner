cmake_minimum_required(VERSION 3.20)

# Pre-project: read CUDA root from config/build.ini to seed CUDA detection
set(BUILD_CONFIG_FILE "${CMAKE_CURRENT_LIST_DIR}/config/build.ini")
if(EXISTS "${BUILD_CONFIG_FILE}")
  file(READ "${BUILD_CONFIG_FILE}" BUILD_INI)
  string(REGEX MATCH "cuda_root[ \t]*=[ \t]*([^\r\n]+)" _cuda_match "${BUILD_INI}")
  if(_cuda_match)
    string(REGEX REPLACE "cuda_root[ \t]*=[ \t]*" "" CUDA_ROOT_STR "${_cuda_match}")
    file(TO_CMAKE_PATH "${CUDA_ROOT_STR}" CUDA_ROOT_CM)
    set(ENV{CUDA_PATH} "${CUDA_ROOT_CM}")
    set(CUDAToolkit_ROOT "${CUDA_ROOT_CM}" CACHE PATH "CUDA Toolkit Root" FORCE)
    if(WIN32)
      set(_NVCC_PATH "${CUDA_ROOT_CM}/bin/nvcc.exe")
    else()
      set(_NVCC_PATH "${CUDA_ROOT_CM}/bin/nvcc")
    endif()
    if(EXISTS "${_NVCC_PATH}")
      set(CMAKE_CUDA_COMPILER "${_NVCC_PATH}" CACHE FILEPATH "nvcc compiler path" FORCE)
      set(ENV{PATH} "${CUDA_ROOT_CM}/bin;$ENV{PATH}")
      message(STATUS "Configured CMAKE_CUDA_COMPILER at ${_NVCC_PATH}")
    endif()
    message(STATUS "Using CUDA from ${CUDA_ROOT_CM} (from config/build.ini)")
  endif()
endif()

project(MyCustomBTCMiner LANGUAGES CXX CUDA)

option(ENABLE_CUDA "Build CUDA components" ON)

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
  tests/test_midstate.cpp
  tests/test_merkle_and_submit.cpp
  tests/test_adapters_compile.cpp
  tests/test_normalizer_caps_and_extranonce.cpp
  tests/test_gbt_adapter.cpp
  tests/test_pool_router.cpp
  tests/test_adapters_policy.cpp
  tests/test_store_persistence.cpp
  tests/test_cuda_launch_plan.cpp
  tests/test_cache_manager.cpp
  tests/test_predict_worker.cpp
  tests/test_cuda_auto_tune.cpp
  tests/test_hit_ring.cpp
  tests/test_pool_profiles.cpp
  # cache tests TBD
)
target_link_libraries(unit_tests
  PRIVATE
    miner_core
    GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(unit_tests)


