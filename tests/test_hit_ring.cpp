#include <gtest/gtest.h>

#include "cuda/hit_ring.h"
#include "cuda/engine.h"

TEST(HitRing, PushPopBasic) {
  cuda_engine::HitRingBuffer ring(2);
  EXPECT_TRUE(ring.empty());
  EXPECT_FALSE(ring.push({1, 10}));
  EXPECT_EQ(ring.size(), 1u);
  auto a = ring.tryPop();
  ASSERT_TRUE(a.has_value());
  EXPECT_EQ(a->work_id, 1ull);
  EXPECT_EQ(a->nonce, 10u);
  EXPECT_TRUE(ring.empty());
}

TEST(HitRing, OverwriteOldestWhenFull) {
  cuda_engine::HitRingBuffer ring(2);
  ring.push({1, 10});
  ring.push({2, 20});
  // Next push overwrites oldest
  bool overwrote = ring.push({3, 30});
  EXPECT_TRUE(overwrote);
  // Pop returns {2,20} then {3,30}
  auto a = ring.tryPop();
  ASSERT_TRUE(a.has_value());
  EXPECT_EQ(a->work_id, 2ull);
  EXPECT_EQ(a->nonce, 20u);
  auto b = ring.tryPop();
  ASSERT_TRUE(b.has_value());
  EXPECT_EQ(b->work_id, 3ull);
  EXPECT_EQ(b->nonce, 30u);
  EXPECT_TRUE(ring.empty());
}

TEST(HitRing, DeviceWriteHitsDemo) {
  // Smoke: ensure we can invoke demo path and collect hits for 3 jobs
  uint64_t work_ids[3] = {101, 102, 103};
  cuda_engine::HitRecord out[3];
  bool ok = cuda_engine::launchWriteHitsDemo(work_ids, 3, 1000, out);
  ASSERT_TRUE(ok);
  EXPECT_EQ(out[0].work_id, 101ull);
  EXPECT_EQ(out[1].work_id, 102ull);
  EXPECT_EQ(out[2].work_id, 103ull);
  EXPECT_EQ(out[0].nonce, 1000u);
  EXPECT_EQ(out[1].nonce, 1001u);
  EXPECT_EQ(out[2].nonce, 1002u);
}



