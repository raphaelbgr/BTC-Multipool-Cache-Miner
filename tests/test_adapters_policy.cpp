#include <gtest/gtest.h>

#include "adapters/stratum_adapter.h"
#include "normalize/targets.h"

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

TEST(AdaptersPolicy, CleanJobsRespectedUnlessForced) {
  adapters::StratumAdapter a("pool");
  a.setCleanJobs(true); // default preference only

  normalize::RawJobInputs in;
  in.source_id = 0;
  in.job.source_name = "pool";
  in.job.work_id = 88;
  in.job.version = 0x20000000u;
  in.job.nbits = 0x1d00ffffu;
  in.job.ntime = 1234;
  in.clean_jobs = false; // server indicated false
  for (int i = 0; i < 32; ++i) { in.prevhash_be[i] = 0; in.merkle_root_be[i] = 0; }
  for (int i = 0; i < 64; ++i) in.header_first64[i] = 0;

  // Not forced: should respect incoming false
  a.ingestJobWithPolicy(in);
  auto w1 = a.pollNormalized();
  ASSERT_TRUE(w1.has_value());
  EXPECT_FALSE(w1->clean_jobs);

  // Forced: override to true
  a.forceCleanJobs(true);
  a.setCleanJobs(true);
  a.ingestJobWithPolicy(in);
  auto w2 = a.pollNormalized();
  ASSERT_TRUE(w2.has_value());
  EXPECT_TRUE(w2->clean_jobs);
}

TEST(AdaptersPolicy, VarDiffAffectsShareTarget) {
  adapters::StratumAdapter a("pool");
  const uint32_t block_nbits = 0x1d00ffffu;
  const uint32_t share_nbits = 0x1f00ffffu;
  a.updateVarDiff(share_nbits);

  normalize::RawJobInputs in;
  in.source_id = 0;
  in.job.source_name = "pool";
  in.job.work_id = 99;
  in.job.version = 0x20000000u;
  in.job.nbits = block_nbits;
  in.job.ntime = 0x5f5e100u;
  for (int i = 0; i < 32; ++i) { in.prevhash_be[i] = 0; in.merkle_root_be[i] = 0; }
  for (int i = 0; i < 64; ++i) in.header_first64[i] = 0;

  a.ingestJobWithPolicy(in);
  auto full = a.pollNormalizedFull();
  ASSERT_TRUE(full.has_value());
  auto expected_block = normalize::compactToTargetU32LE(block_nbits);
  auto expected_share = normalize::compactToTargetU32LE(share_nbits);
  EXPECT_EQ(full->item.block_target_le, expected_block);
  EXPECT_EQ(full->item.share_target_le, expected_share);
  // Targets should differ when varDiff is set
  EXPECT_NE(full->item.block_target_le, full->item.share_target_le);
}



