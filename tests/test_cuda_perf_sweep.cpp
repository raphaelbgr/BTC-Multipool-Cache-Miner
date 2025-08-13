#include <gtest/gtest.h>
#include <cstdint>
#include <algorithm>
#include <vector>

#include "cuda/engine.h"

TEST(CUDAPerf, ComputeLaunchPlanBasicRanges) {
  // Smoke test the computeLaunchPlan helper across ranges of desired threads
  for (uint64_t desired : {64ull, 128ull, 256ull, 512ull, 1024ull, 2048ull}) {
    auto plan = cuda_engine::computeLaunchPlan(8, desired);
    EXPECT_GE(plan.num_jobs, 1u);
    EXPECT_GE(plan.blocks_per_job, 1u);
    EXPECT_GE(plan.threads_per_block, 1u);
    EXPECT_LE(plan.threads_per_block, 1024u);
  }
}




