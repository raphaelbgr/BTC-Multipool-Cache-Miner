#include <gtest/gtest.h>

#include "cuda/auto_tune.h"
#include "cuda/engine.h"

TEST(CUDAAutoTune, IncreaseWhenUnderBudget) {
  cuda_engine::AutoTuneInputs in{4, 16, 8}; // 4ms observed, 16ms budget
  auto d = cuda_engine::autoTuneMicroBatch(in);
  EXPECT_TRUE(d.increased);
  EXPECT_EQ(d.nextMicroBatch, 16u);
}

TEST(CUDAAutoTune, DecreaseWhenOverBudget) {
  cuda_engine::AutoTuneInputs in{20, 16, 8};
  auto d = cuda_engine::autoTuneMicroBatch(in);
  EXPECT_TRUE(d.decreased);
  EXPECT_EQ(d.nextMicroBatch, 4u);
}

TEST(CUDAAutoTune, NoChangeNearBudget) {
  cuda_engine::AutoTuneInputs in{15, 16, 8};
  auto d = cuda_engine::autoTuneMicroBatch(in);
  EXPECT_FALSE(d.increased);
  EXPECT_FALSE(d.decreased);
  EXPECT_EQ(d.nextMicroBatch, 8u);
}

TEST(CUDALaunch, MultiJobStubLaunch) {
  using namespace cuda_engine;
  // This will return false on non-CUDA builds; true when CUDA present
  bool ok = launchMultiJobStub(3, 1024);
  (void)ok; // Ensure it links and callable across builds
  SUCCEED();
}


