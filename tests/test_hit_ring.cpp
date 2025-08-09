#include <gtest/gtest.h>

#include "cuda/hit_ring.h"

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


