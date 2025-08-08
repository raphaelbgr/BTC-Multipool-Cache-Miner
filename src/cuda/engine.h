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

inline void launchStub(const LaunchParams&) {
  // No-op when CUDA is disabled.
}

}


