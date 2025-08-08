#include "cuda/engine.h"
#include "cuda/sha256d.cuh"

#include <cuda_runtime.h>

namespace cuda_engine {

__global__ void kernel_noop() {
  cuda_sha256d::hash256_once_stub();
}

void launchStub(const LaunchParams& params) {
  if (params.blocks == 0 || params.threads_per_block == 0) return;
  kernel_noop<<<params.blocks, params.threads_per_block>>>();
  cudaDeviceSynchronize();
}

}  // namespace cuda_engine


