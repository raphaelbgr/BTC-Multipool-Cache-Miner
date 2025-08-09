#include <gtest/gtest.h>

#include "adapters/gbt_adapter.h"

TEST(GBTAdapter, IngestAndPoll) {
  adapters::GbtAdapter a("gbt");
  normalize::RawJobInputs in;
  in.source_id = 2;
  in.job.source_name = "gbt";
  in.job.work_id = 5001;
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

  a.ingestTemplate(in);
  auto w = a.pollNormalized();
  ASSERT_TRUE(w.has_value());
  EXPECT_EQ(w->work_id, 5001u);
  EXPECT_EQ(w->source_id, 2u);
}


