#include <gtest/gtest.h>

#include "adapters/gbt_adapter.h"
#include "submit/gbt_submitter.h"

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

TEST(GBTSubmitter, BuildBlockHexScaffold) {
  config::PoolEntry::RpcConfig rc; rc.url = "http://127.0.0.1:8332"; rc.auth = "cookie";
  submit::GbtSubmitter sub(rc);
  uint8_t header[80] = {0};
  std::string hex;
  ASSERT_TRUE(sub.buildBlockHex(header, &hex));
  ASSERT_GE(hex.size(), 162u); // 80B header (160 hex) + at least 1-byte varint (00)
}

TEST(GBTSubmitter, BuildMinimalWitnessCommitmentCoinbase) {
  std::string tx;
  // 32-byte zero commitment for test
  std::string comm(64, '0');
  ASSERT_TRUE(submit::GbtSubmitter::buildMinimalWitnessCommitmentCoinbase(0, comm, &tx));
  // Version (4 bytes) + 1 input + 1 output + locktime should be present
  ASSERT_GE(tx.size(), 100u);
  // Contains OP_RETURN 0x6a and tag aa21a9ed
  ASSERT_NE(tx.find("6a26aa21a9ed"), std::string::npos);
}


