add_library(miner_core
  obs/log.cc
  obs/metrics.cc
  normalize/endianness.cc
  normalize/targets.cc
  normalize/merkle.cc
  normalize/midstate.cc
  registry/work_source_registry.cc
  config/config.cc
  submit/cpu_verify.cc
  submit/submit_router.cc
)

target_include_directories(miner_core PUBLIC ${CMAKE_CURRENT_LIST_DIR})

target_link_libraries(miner_core PUBLIC nlohmann_json::nlohmann_json)

if(ENABLE_CUDA)
  enable_language(CUDA)
  add_library(miner_cuda
    cuda/engine.cu
  )
  target_include_directories(miner_cuda PUBLIC ${CMAKE_CURRENT_LIST_DIR})
  target_link_libraries(miner_core PUBLIC miner_cuda)
endif()


