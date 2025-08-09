#include <gtest/gtest.h>
#include "adapters/adapter_base.h"
#include "adapters/stratum_adapter.h"
#include "normalize/normalizer.h"
#include "registry/work_source_registry.h"

TEST(Adapters, StratumAdapterEnqueuePoll) {
  adapters::StratumAdapter a("pool");
  a.enqueueDummy(1, 0x1d00ffffu);
  auto w = a.pollNormalized();
  ASSERT_TRUE(w.has_value());
  EXPECT_EQ(w->work_id, 1u);
}

TEST(Adapters, StratumAdapterIngestVarDiffAndCleanJobs) {
  adapters::StratumAdapter a("pool");
  normalize::RawJobInputs in;
  in.source_id = 1;
  in.job.source_name = "pool";
  in.job.work_id = 42;
  in.job.version = 0x20000000u;
  in.job.nbits = 0x1b0404cbu; // sample block target
  in.job.ntime = 0x5f5e100u;
  in.share_nbits = 0x1f0fffffu; // easy share target to differ from block
  in.vmask = 0x1fffe000; // example version rolling mask
  in.ntime_min = in.job.ntime;
  in.ntime_max = in.job.ntime + 2;
  in.extranonce2_size = 4;
  in.clean_jobs = true;
  for (int i = 0; i < 32; ++i) {
    in.prevhash_be[i] = static_cast<uint8_t>(i);
    in.merkle_root_be[i] = static_cast<uint8_t>(31 - i);
  }
  for (int i = 0; i < 64; ++i) in.header_first64[i] = 0;

  a.ingestJob(in);
  auto w = a.pollNormalized();
  ASSERT_TRUE(w.has_value());
  EXPECT_EQ(w->work_id, 42u);
  EXPECT_EQ(w->source_id, 1u);
  EXPECT_EQ(w->version, 0x20000000u);
  EXPECT_EQ(w->nbits, 0x1b0404cbu);
  EXPECT_EQ(w->vmask, 0x1fffe000u);
  EXPECT_EQ(w->extranonce2_size, 4u);
  EXPECT_TRUE(w->clean_jobs);
}

TEST(Adapters, StratumAdapterStateMachine) {
  adapters::StratumAdapter a("pool");
  EXPECT_EQ(static_cast<int>(a.state()), static_cast<int>(adapters::StratumAdapter::State::kDisconnected));
  a.connect();
  EXPECT_EQ(static_cast<int>(a.state()), static_cast<int>(adapters::StratumAdapter::State::kConnecting));
  a.onSubscribed("abcd", 8);
  EXPECT_EQ(static_cast<int>(a.state()), static_cast<int>(adapters::StratumAdapter::State::kSubscribed));
  EXPECT_EQ(a.extranonce2Size(), 8);
  a.onAuthorizeOk();
  EXPECT_EQ(static_cast<int>(a.state()), static_cast<int>(adapters::StratumAdapter::State::kAuthorized));
}

TEST(Adapters, AdapterToRegistryIntegration) {
  adapters::StratumAdapter a("pool");
  registry::WorkSourceRegistry reg(2);

  // Prepare one normalized job and push it
  normalize::RawJobInputs in;
  in.source_id = 0;
  in.job.source_name = "pool";
  in.job.work_id = 1001;
  in.job.version = 0x20000000u;
  in.job.nbits = 0x1d00ffffu;
  in.job.ntime = 0x5f5e100u;
  in.share_nbits = 0x1e00ffffu;
  in.vmask = 0x1fffe000u;
  in.ntime_min = in.job.ntime;
  in.ntime_max = in.job.ntime + 1;
  in.extranonce2_size = 4;
  in.clean_jobs = true;
  for (int i = 0; i < 32; ++i) {
    in.prevhash_be[i] = static_cast<uint8_t>(i);
    in.merkle_root_be[i] = static_cast<uint8_t>(31 - i);
  }
  for (int i = 0; i < 64; ++i) in.header_first64[i] = 0;

  a.ingestJob(in);
  auto full = a.pollNormalizedFull();
  ASSERT_TRUE(full.has_value());

  auto before = reg.get(0);
  uint64_t gen_before = before ? before->gen : 0;
  reg.set(0, full->item, full->job_const);
  auto after = reg.get(0);
  ASSERT_TRUE(after.has_value());
  EXPECT_GT(after->gen, gen_before);
  EXPECT_EQ(after->item.work_id, 1001u);
  EXPECT_EQ(after->item.source_id, 0u);
  EXPECT_EQ(after->item.extranonce2_size, 4u);
  EXPECT_TRUE(after->item.clean_jobs);
}



