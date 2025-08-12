#pragma once

#include <cstdint>

namespace cuda_engine {

struct LaunchParams {
  uint32_t blocks{0};
  uint32_t threads_per_block{0};
};

// Forward-declare launch plan utilities
struct MultiJobLaunchPlan;
MultiJobLaunchPlan computeLaunchPlan(uint32_t num_jobs, uint64_t desired_threads_per_job);

inline bool isCudaAvailable() {
#ifdef __CUDACC__
  return true;
#else
  return false;
#endif
}

// Implemented in engine.cu when CUDA is enabled
void launchStub(const LaunchParams& params);

// Launch a multi-job stub kernel with grid.y = num_jobs and grid.x = blocks per job
// threads_per_block is taken from the provided plan.
// Returns false if plan is empty or CUDA not available (in non-CUDA builds).
bool launchMultiJobStub(uint32_t num_jobs, uint64_t desired_threads_per_job);

// Demo: device writes one hit per job using provided work_ids and a base nonce.
// Writes exactly num_jobs records into out_hits_host.
bool launchWriteHitsDemo(const uint64_t* work_ids_host,
                         uint32_t num_jobs,
                         uint32_t nonce_base,
                         struct HitRecord* out_hits_host);

}
