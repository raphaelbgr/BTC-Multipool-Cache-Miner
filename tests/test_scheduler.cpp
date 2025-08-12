#include <gtest/gtest.h>

#include "scheduler/scheduler.h"

TEST(Scheduler, WeightsReplicateIdsWithCap) {
  scheduler::SchedulerState s;
  std::vector<uint64_t> ids = {10, 20, 30};
  std::unordered_map<uint64_t, registry::WorkSlotSnapshot> snaps;
  registry::WorkSlotSnapshot a{}; a.item.work_id = 10; a.item.source_id = 1;
  registry::WorkSlotSnapshot b{}; b.item.work_id = 20; b.item.source_id = 2;
  registry::WorkSlotSnapshot c{}; c.item.work_id = 30; c.item.source_id = 1;
  snaps.emplace(10, a); snaps.emplace(20, b); snaps.emplace(30, c);
  s.source_weight[1] = 3; // sources 1 will appear 3 times per id
  s.source_weight[2] = 1;
  auto sel = s.select(ids, snaps, 10);
  // Expected: 10,10,10,20,30,30,30 (order preserved with replication)
  ASSERT_GE(sel.size(), 7u);
  EXPECT_EQ(sel[0], 10ull);
  EXPECT_EQ(sel[1], 10ull);
  EXPECT_EQ(sel[2], 10ull);
  EXPECT_EQ(sel[3], 20ull);
  EXPECT_EQ(sel[4], 30ull);
}


