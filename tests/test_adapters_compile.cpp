#include <gtest/gtest.h>
#include "adapters/adapter_base.h"
#include "adapters/stratum_adapter.h"

TEST(Adapters, StratumAdapterEnqueuePoll) {
  adapters::StratumAdapter a("pool");
  a.enqueueDummy(1, 0x1d00ffffu);
  auto w = a.pollNormalized();
  ASSERT_TRUE(w.has_value());
  EXPECT_EQ(w->work_id, 1u);
}



