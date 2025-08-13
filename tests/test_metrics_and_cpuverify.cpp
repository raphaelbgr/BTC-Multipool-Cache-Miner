#include <gtest/gtest.h>
#include "obs/metrics.h"
#include "submit/cpu_verify.h"
#include "normalize/targets.h"
#include "config/config.h"

TEST(Metrics, Snapshot) {
  obs::MetricsRegistry m;
  m.increment("a");
  m.increment("a", 2);
  m.setGauge("g", -5);
  auto j = m.snapshot();
  ASSERT_TRUE(j.contains("counters"));
  ASSERT_TRUE(j.contains("gauges"));
  EXPECT_EQ(j["counters"]["a"], 3);
  EXPECT_EQ(j["gauges"]["g"], -5);
}

TEST(Config, LoadsDefaultsAndParsesNewSections) {
  auto cfg = config::loadDefault();
  EXPECT_GE(cfg.pools.size(), 1u);
  EXPECT_FALSE(cfg.outbox.path.empty());
  EXPECT_EQ(cfg.outbox.rotate_interval_sec, 0ull);
  EXPECT_FALSE(cfg.ledger.path.empty());
  EXPECT_GT(cfg.ledger.max_bytes, 0ull);
}

TEST(CPUVerify, Sha256d_KnownVector) {
  // Test vector: SHA256d("")
  const uint8_t empty[1] = {};
  uint8_t out[32];
  submit::sha256d(empty, 0, out);
  const char* expect_hex = "5df6e0e2761359d30a8275058e299fcc0381534545f55cf43e41983f5d4c9456";
  auto tohex = [](const uint8_t* b){
    static const char* k = "0123456789abcdef";
    std::string s; s.resize(64);
    for (int i = 0; i < 32; ++i) { s[i*2]=k[(b[i]>>4)&15]; s[i*2+1]=k[b[i]&15]; }
    return s;
  };
  EXPECT_EQ(tohex(out), std::string(expect_hex));
}

TEST(CPUVerify, CompareLEQTarget) {
  // Hash equal to target should pass; slightly larger should fail.
  uint8_t hash_be[32] = {};
  // Set hash to 0x0000...00FF (least) to be larger than a small target
  hash_be[31] = 0xFF;
  // Target from compact difficulty that's smaller than hash (so compare should be false)
  auto target = normalize::compactToTargetU32LE(0x03000001u);
  EXPECT_FALSE(submit::isHashLEQTarget(hash_be, target));
  // Set hash to zero -> always <= target
  std::memset(hash_be, 0, 32);
  EXPECT_TRUE(submit::isHashLEQTarget(hash_be, target));
}


