#pragma once

#include <algorithm>
#include <cstdint>

namespace cuda_engine {

struct MultiJobLaunchPlan {
  uint32_t blocks_per_job{0};
  uint32_t threads_per_block{0};
  uint32_t num_jobs{0};
};

inline uint32_t clampThreadsPerBlock(uint32_t requested) {
  const uint32_t kMin = 64;   // at least two warps
  const uint32_t kMax = 1024; // common CUDA max
  if (requested < kMin) return kMin;
  if (requested > kMax) return kMax;
  return requested;
}

// Compute a simple y=jobs, x=blocks plan given desired parallelism per job.
// We try to keep threads_per_block a power-of-two between [64,1024].
inline MultiJobLaunchPlan computeLaunchPlan(uint32_t num_jobs,
                                           uint64_t desired_threads_per_job) {
  MultiJobLaunchPlan plan{};
  plan.num_jobs = num_jobs;
  if (num_jobs == 0 || desired_threads_per_job == 0) return plan;

  // Choose threads_per_block as the largest power-of-two <= 256 (or <= desired), then clamp.
  uint32_t tpb = 256;
  while (tpb > desired_threads_per_job && tpb >= 64) tpb >>= 1;
  tpb = clampThreadsPerBlock(tpb);
  plan.threads_per_block = tpb;

  // Blocks per job is ceil(desired / tpb)
  uint64_t blocks = (desired_threads_per_job + tpb - 1) / tpb;
  if (blocks == 0) blocks = 1;
  if (blocks > 0xFFFFFFFFull) blocks = 0xFFFFFFFFull;
  plan.blocks_per_job = static_cast<uint32_t>(blocks);
  return plan;
}

}  // namespace cuda_engine


