#include <gtest/gtest.h>

#include "cache/predict_worker.h"

TEST(Predict, ThrottleOnHighUtilOrLowFree) {
  cache::PredictabilityWorker pw({85, 256});
  cache::GpuTelemetry t{8192, 7800, 96};
  auto d = pw.evaluate(t);
  EXPECT_TRUE(d.throttle);
  EXPECT_FALSE(d.allowAllocate);
}

TEST(Predict, AllowAllocateWhenFreeAndUtilOk) {
  cache::PredictabilityWorker pw({85, 256});
  cache::GpuTelemetry t{8192, 7600, 80};
  auto d = pw.evaluate(t);
  EXPECT_FALSE(d.throttle);
  EXPECT_TRUE(d.allowAllocate);
  EXPECT_TRUE(d.allowRelease);
}

TEST(Predict, DiscourageReleaseWhenAmpleFreeAndLowUtil) {
  cache::PredictabilityWorker pw({85, 256});
  cache::GpuTelemetry t{8192, 7000, 60};
  auto d = pw.evaluate(t);
  EXPECT_FALSE(d.throttle);
  EXPECT_TRUE(d.allowAllocate);
  EXPECT_FALSE(d.allowRelease);
}


