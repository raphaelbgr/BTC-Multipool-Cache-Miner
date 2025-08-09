#include <gtest/gtest.h>

#include "adapters/pool_router.h"
#include "adapters/stratum_adapter.h"

TEST(PoolRouter, FailoverPicksFirstWithWork) {
  auto a1 = std::make_shared<adapters::StratumAdapter>("p1");
  auto a2 = std::make_shared<adapters::StratumAdapter>("p2");
  auto a3 = std::make_shared<adapters::StratumAdapter>("p3");

  adapters::PoolRouter r(adapters::PoolRouter::Strategy::kFailover);
  r.addAdapter(a1);
  r.addAdapter(a2);
  r.addAdapter(a3);

  a2->enqueueDummy(200, 0x1d00ffffu);
  a3->enqueueDummy(300, 0x1d00ffffu);

  auto w = r.pollNext();
  ASSERT_TRUE(w.has_value());
  EXPECT_EQ(w->work_id, 200u);
}

TEST(PoolRouter, RoundRobinAlternates) {
  auto a1 = std::make_shared<adapters::StratumAdapter>("p1");
  auto a2 = std::make_shared<adapters::StratumAdapter>("p2");

  adapters::PoolRouter r(adapters::PoolRouter::Strategy::kRoundRobin);
  r.addAdapter(a1);
  r.addAdapter(a2);

  // Seed both with one work item
  a1->enqueueDummy(101, 0x1d00ffffu);
  a2->enqueueDummy(102, 0x1d00ffffu);

  auto w1 = r.pollNext();
  auto w2 = r.pollNext();
  ASSERT_TRUE(w1.has_value());
  ASSERT_TRUE(w2.has_value());
  // Should see both items in some alternating order
  EXPECT_NE(w1->work_id, w2->work_id);
}


