#pragma once

#include <cstdint>

namespace cuda_engine {

struct LaunchParams {
  uint32_t blocks{0};
  uint32_t threads_per_block{0};
};

inline bool isCudaAvailable() {
#ifdef __CUDACC__
  return true;
#else
  return false;
#endif
}

// Implemented in engine.cu when CUDA is enabled
void launchStub(const LaunchParams& params);

}
