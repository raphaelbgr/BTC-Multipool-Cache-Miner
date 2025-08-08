#pragma once

#ifdef __CUDACC__
#define MCBCM_DEVICE __device__
#define MCBCM_GLOBAL __global__
#else
#define MCBCM_DEVICE
#define MCBCM_GLOBAL
#endif

namespace cuda_sha256d {

// Placeholder for future device implementation
MCBCM_DEVICE inline void hash256_once_stub() {}

}


