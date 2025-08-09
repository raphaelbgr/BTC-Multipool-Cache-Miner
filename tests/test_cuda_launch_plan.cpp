#include <gtest/gtest.h>

#include "cuda/launch_plan.h"

TEST(CUDALaunch, ClampThreads) {
  using namespace cuda_engine;
  EXPECT_EQ(clampThreadsPerBlock(1), 64u);
  EXPECT_EQ(clampThreadsPerBlock(64), 64u);
  EXPECT_EQ(clampThreadsPerBlock(1024), 1024u);
  EXPECT_EQ(clampThreadsPerBlock(5000), 1024u);
}

TEST(CUDALaunch, ComputePlanBasic) {
  using namespace cuda_engine;
  auto p = computeLaunchPlan(3, 1000);
  EXPECT_EQ(p.num_jobs, 3u);
  EXPECT_EQ(p.threads_per_block, 256u);
  EXPECT_GT(p.blocks_per_job, 0u);
}


