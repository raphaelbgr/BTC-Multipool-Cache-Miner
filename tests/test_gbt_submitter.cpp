#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "submit/gbt_submitter.h"

TEST(GBTSubmitter, SubmitRequiresCoinbase) {
  config::PoolEntry::RpcConfig rc; rc.url = "http://127.0.0.1:8332"; rc.auth = "cookie";
  submit::GbtSubmitter sub(rc);
  uint8_t header[80] = {0};
  // Without coinbase, submitHeader should bail early
  EXPECT_FALSE(sub.submitHeader(header));
}

TEST(GBTSubmitter, SynthCoinbaseFlagControlsBuild) {
  // Build a fake GBT with default_witness_commitment and height
  nlohmann::json gbt;
  gbt["default_witness_commitment"] = std::string(64, '0');
  gbt["height"] = 100;

  config::PoolEntry::RpcConfig rc; rc.url = "http://127.0.0.1:8332"; rc.auth = "cookie";
  // Case 1: no synth allowed -> no coinbase built
  submit::GbtSubmitter sub1(rc);
  sub1.setAllowSynthCoinbase(false);
  sub1.updateTemplate(gbt);
  EXPECT_FALSE(sub1.hasCoinbase());

  // Case 2: synth allowed -> coinbase built
  submit::GbtSubmitter sub2(rc);
  sub2.setAllowSynthCoinbase(true);
  sub2.updateTemplate(gbt);
  EXPECT_TRUE(sub2.hasCoinbase());
}

TEST(GBTSubmitter, BuildCoinbaseTxWithPayoutAndCommitment) {
  std::string tx;
  std::string comm(64, '0');
  // P2PKH script: OP_DUP OP_HASH160 <20-byte zero> OP_EQUALVERIFY OP_CHECKSIG
  std::string payout = "76a914" + std::string(40, '0') + "88ac";
  ASSERT_TRUE(submit::GbtSubmitter::buildCoinbaseTx(1, payout, 5000000000ull, comm, "", &tx));
  ASSERT_NE(tx.find("76a914"), std::string::npos);
  ASSERT_NE(tx.find("6a24aa21a9ed"), std::string::npos);
}


