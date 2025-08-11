add_library(miner_core
  obs/log.cc
  obs/metrics.cc
  normalize/endianness.cc
  normalize/targets.cc
  normalize/merkle.cc
  normalize/midstate.cc
  normalize/normalizer.cc
  normalize/coinbase.cc
  store/ledger.h
  store/outbox.h
  registry/work_source_registry.cc
  config/config.cc
  submit/cpu_verify.cc
  submit/submit_router.cc
  submit/stratum_submitter.h
  adapters/stratum_adapter.cc
  adapters/gbt_adapter.cc
  adapters/gbt_runner.cc
  adapters/pool_router.cc
  adapters/pool_profiles.cc
  adapters/stratum_runner.cc
  cache/vram_pages.cc
  cache/cache_manager.cc
  net/stratum_client.cc
  net/rpc_client.cc
)

# OS-specific socket backends
if (WIN32)
  target_sources(miner_core PRIVATE net/socket_win.cc)
else()
  target_sources(miner_core PRIVATE net/socket_posix.cc)
endif()

target_include_directories(miner_core PUBLIC ${CMAKE_CURRENT_LIST_DIR})

target_link_libraries(miner_core PUBLIC nlohmann_json::nlohmann_json)

# Optional OpenSSL TLS backend (POSIX)
find_package(OpenSSL QUIET)
if(OpenSSL_FOUND)
  target_sources(miner_core PRIVATE net/socket_openssl.cc)
  target_compile_definitions(miner_core PRIVATE USE_OPENSSL)
  target_link_libraries(miner_core PRIVATE OpenSSL::SSL OpenSSL::Crypto)
endif()

if(NOT ENABLE_CUDA)
  message(FATAL_ERROR "ENABLE_CUDA is OFF but this project requires CUDA. Set -DENABLE_CUDA=ON and ensure CUDA toolkit is installed.")
endif()

if(NOT DEFINED CMAKE_CUDA_HOST_COMPILER AND DEFINED CMAKE_C_COMPILER)
  set(CMAKE_CUDA_HOST_COMPILER "${CMAKE_C_COMPILER}" CACHE FILEPATH "CUDA host compiler" FORCE)
endif()

enable_language(CUDA)
add_library(miner_cuda
  cuda/engine.cu
)
set_target_properties(miner_cuda PROPERTIES
  CUDA_SEPARABLE_COMPILATION ON
  CUDA_STANDARD 14
  CUDA_STANDARD_REQUIRED ON
)
if(NOT DEFINED CMAKE_CUDA_ARCHITECTURES)
  # Default to common arch; adjust as needed
  set(CMAKE_CUDA_ARCHITECTURES 86 CACHE STRING "CUDA arch list" FORCE)
endif()
target_include_directories(miner_cuda PUBLIC ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(miner_core PUBLIC miner_cuda)
message(STATUS "CUDA enabled and required")

option(BUILD_TOOLS "Build optional CLI tools (diagnostics)" OFF)
if(BUILD_TOOLS)
  add_executable(stratum_probe
    tools/stratum_probe.cpp
  )
  target_link_libraries(stratum_probe PRIVATE miner_core)
endif()

add_executable(stratum_registry_runner
  tools/stratum_registry_runner.cpp
)
target_link_libraries(stratum_registry_runner PRIVATE miner_core)


