#include <gtest/gtest.h>

#include "adapters/stratum_adapter.h"

TEST(AdaptersPolicy, VarDiffAndCapsApplied) {
  adapters::StratumAdapter a("pool");
  a.updateVarDiff(0x1f0fffffu);
  a.setVersionMask(0x1fffe000u);
  a.setNtimeCaps(1000, 1002);
  a.setCleanJobs(true);

  normalize::RawJobInputs in;
  in.source_id = 1;
  in.job.source_name = "pool";
  in.job.work_id = 77;
  in.job.version = 0x20000000u;
  in.job.nbits = 0x1d00ffffu; // block target
  in.job.ntime = 999; // below min
  for (int i = 0; i < 32; ++i) { in.prevhash_be[i] = 0; in.merkle_root_be[i] = 0; }
  for (int i = 0; i < 64; ++i) in.header_first64[i] = 0;

  a.ingestJobWithPolicy(in);
  auto w = a.pollNormalized();
  ASSERT_TRUE(w.has_value());
  EXPECT_EQ(w->work_id, 77u);
  EXPECT_EQ(w->source_id, 1u);
  EXPECT_EQ(w->vmask, 0x1fffe000u);
  EXPECT_EQ(w->extranonce2_size, 0u); // default unchanged
  EXPECT_TRUE(w->clean_jobs);
  // Ntime was clamped by policy caps (normalize sets min/max in item)
  EXPECT_EQ(w->ntime_min, 1000u);
  EXPECT_EQ(w->ntime_max, 1002u);
}



