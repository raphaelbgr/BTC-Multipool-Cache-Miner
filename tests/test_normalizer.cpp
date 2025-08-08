#include <gtest/gtest.h>
#include "normalize/normalizer.h"

TEST(Normalizer, BasicFields) {
  normalize::RawJobInputs in;
  in.source_id = 2;
  in.job.source_name = "pool";
  in.job.work_id = 123;
  in.job.version = 0x20000000u;
  in.job.nbits = 0x1d00ffffu;
  in.job.ntime = 0x5f5e100u; // arbitrary
  in.share_nbits = 0x207fffffu; // very high target -> FFFFFFFF...

  // prevhash and merkle root: set to simple patterns
  for (int i = 0; i < 32; ++i) {
    in.prevhash_be[i] = static_cast<uint8_t>(i);
    in.merkle_root_be[i] = static_cast<uint8_t>(31 - i);
  }
  // For the test, fill header_first64 with zeros
  for (int i = 0; i < 64; ++i) in.header_first64[i] = 0;

  auto out = normalize::normalizeJob(in);
  ASSERT_TRUE(out.has_value());
  const auto &w = out->item;
  EXPECT_EQ(w.work_id, 123u);
  EXPECT_EQ(w.source_id, 2u);
  EXPECT_EQ(w.version, 0x20000000u);
  EXPECT_EQ(w.nbits, 0x1d00ffffu);
  EXPECT_TRUE(w.active);
  // Targets should be set (non-zero for genesis compact)
  bool any_nonzero = false;
  for (int i = 0; i < 8; ++i) any_nonzero = any_nonzero || (w.block_target_le[i] != 0u);
  EXPECT_TRUE(any_nonzero);
}


