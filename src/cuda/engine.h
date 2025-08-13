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
  uint8_t  share_target_be[32]{}; // precomputed BE target for device compare
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

// Launch mining stub with an explicit plan (blocks_per_job x threads_per_block), grid.y = num_jobs
bool launchMineWithPlan(uint32_t num_jobs,
                        uint32_t blocks_per_job,
                        uint32_t threads_per_block,
                        uint32_t nonce_base);

// Launch mining with plan and micro-batch of nonces per thread
bool launchMineWithPlanBatch(uint32_t num_jobs,
                             uint32_t blocks_per_job,
                             uint32_t threads_per_block,
                             uint32_t nonce_base,
                             uint32_t nonces_per_thread);

// Host-side hint whether jobs are stored in constant memory (fast path)
bool usingConstJobs();

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

// Query device memory info (bytes). Returns false if CUDA not available
bool getDeviceMemoryInfo(uint64_t* out_free_bytes, uint64_t* out_total_bytes);

  // Query kernel occupancy for the mining batch kernel at a given threads_per_block.
  // Returns false if CUDA not available. Outputs:
  //  - out_occupancy_0_1: active_threads_per_sm / max_threads_per_sm, clamped to [0,1]
  //  - out_sms: number of SMs on the device
  //  - out_active_blocks_per_sm: active blocks per SM for the kernel
  //  - out_max_threads_per_sm: hardware limit of threads per SM
  bool getMineBatchOccupancy(uint32_t threads_per_block,
                             float* out_occupancy_0_1,
                             int* out_sms,
                             int* out_active_blocks_per_sm,
                             int* out_max_threads_per_sm);

  // Read and reset the device-side per-block flush counter (diagnostics). Returns false if CUDA not available.
  bool readAndResetBlockFlushCount(uint32_t* out_count);

  // Read kernel attributes for mining batch kernel. Returns false if CUDA not available.
  bool getMineBatchKernelAttrs(int* out_num_regs, size_t* out_shared_bytes, int* out_max_threads_per_block);

  // Read and reset per-SM flush counts into out_counts[sm_count]. Returns false if CUDA not available.
  bool readAndResetSmFlushCounts(uint32_t sm_count, uint32_t* out_counts);

}
