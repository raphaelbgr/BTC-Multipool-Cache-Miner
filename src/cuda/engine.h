#pragma once

#include <cstdint>

namespace cuda_engine {

struct LaunchParams {
  uint32_t blocks{0};
  uint32_t threads_per_block{0};
};

// Minimal per-job data for device header assembly scaffolding
struct alignas(64) DeviceJob {
  uint32_t version{0};
  uint32_t ntime{0};
  uint32_t nbits{0};
  uint32_t vmask{0};
  uint32_t ntime_min{0};
  uint32_t ntime_max{0};
  uint8_t extranonce2_size{0};
  uint8_t _pad[3]{};
  uint64_t work_id{0};
  uint32_t prevhash_le[8]{};
  uint32_t merkle_root_le[8]{};
  uint32_t share_target_le[8]{};
  uint32_t block_target_le[8]{};
  uint32_t midstate_le[8]{}; // precomputed SHA-256 state after first 64 bytes
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

// Upload per-job constants for the kernel (scaffold). Replaces previous table.
bool uploadDeviceJobs(const DeviceJob* jobs_host, uint32_t num_jobs);

// Launch mining stub over uploaded jobs: one candidate per job with given nonce base
bool launchMineStub(uint32_t num_jobs, uint32_t nonce_base);

// Compute SHA-256d(header80) on device for a given uploaded job and nonce; writes 32-byte BE digest to out32_host
bool computeDeviceHashForJob(uint32_t job_index, uint32_t nonce, unsigned char out32_host[32]);

// Demo: device writes one hit per job using provided work_ids and a base nonce.
// Writes exactly num_jobs records into out_hits_host.
bool launchWriteHitsDemo(const uint64_t* work_ids_host,
                         uint32_t num_jobs,
                         uint32_t nonce_base,
                         struct HitRecord* out_hits_host);

// Device hit ring lifecycle and push API (simple global buffer per device)
bool initDeviceHitBuffer(uint32_t capacity);
void freeDeviceHitBuffer();
// Launch a tiny kernel that pushes one hit per job into the device ring
bool launchPushHitsToDeviceRing(const uint64_t* work_ids_host,
                                uint32_t num_jobs,
                                uint32_t nonce_base);
// Drain up to max_out hits into host buffer; returns number drained via out_count
bool drainDeviceHits(struct HitRecord* out_hits_host,
                     uint32_t max_out,
                     uint32_t* out_count);

}
