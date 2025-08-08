#include <gtest/gtest.h>
#include "registry/work_source_registry.h"

TEST(Registry, SetGetAndGenBump) {
  registry::WorkSourceRegistry reg(2);
  registry::WorkItem item;
  item.work_id = 42;
  reg.set(0, item);

  auto snap1 = reg.get(0);
  ASSERT_TRUE(snap1.has_value());
  EXPECT_EQ(snap1->item.work_id, 42u);
  auto gen1 = snap1->gen;

  item.work_id = 43;
  reg.set(0, item);
  auto snap2 = reg.get(0);
  ASSERT_TRUE(snap2.has_value());
  EXPECT_EQ(snap2->item.work_id, 43u);
  EXPECT_GT(snap2->gen, gen1);
}


