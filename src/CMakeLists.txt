add_library(miner_core
  obs/log.cc
  obs/metrics.cc
  normalize/endianness.cc
  normalize/targets.cc
  normalize/merkle.cc
  normalize/midstate.cc
  normalize/normalizer.cc
  registry/work_source_registry.cc
  config/config.cc
  submit/cpu_verify.cc
  submit/submit_router.cc
  adapters/stratum_adapter.cc
)

target_include_directories(miner_core PUBLIC ${CMAKE_CURRENT_LIST_DIR})

target_link_libraries(miner_core PUBLIC nlohmann_json::nlohmann_json)

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


