#include <gtest/gtest.h>
#include <vector>
#include <string>

#include "normalize/rolling_caps.h"
#include "normalize/coinbase.h"

TEST(NormalizerCaps, ClampVersionAndNtime) {
  // Version rolling: vmask permits bits 13..29 (~0x1fffe000)
  uint32_t base_version = 0x20000000u;
  uint32_t vmask = 0x1fffe000u;
  // Proposed version flips some allowed bits; clamp should preserve base high bits
  uint32_t proposed = base_version | 0x0000a000u;
  uint32_t clamped = normalize::clampRollingVersion(proposed, vmask);
  EXPECT_EQ(clamped & ~vmask, base_version & ~vmask);

  // Ntime clamp
  uint32_t ntime = 1000;
  EXPECT_EQ(normalize::clampNtime(ntime, 1000, 1002), 1000u);
  EXPECT_EQ(normalize::clampNtime(1010, 1000, 1002), 1002u);
  EXPECT_EQ(normalize::clampNtime(999, 1000, 1002), 1000u);
}

TEST(Extranonce, CoinbaseAssembly) {
  normalize::CoinbaseParts parts;
  parts.prefix = {0x01, 0x02};
  parts.suffix = {0xaa, 0xbb};
  std::string extranonce1 = "abcd"; // raw bytes used as-is
  std::vector<uint8_t> extranonce2 = {0x10, 0x11, 0x12, 0x13};
  auto tx = normalize::assembleCoinbaseTx(parts, extranonce1, extranonce2);
  ASSERT_EQ(tx.size(), parts.prefix.size() + extranonce1.size() + extranonce2.size() + parts.suffix.size());
  // Check ordering
  EXPECT_EQ(tx[0], 0x01);
  EXPECT_EQ(tx[1], 0x02);
  EXPECT_EQ(tx[2], static_cast<uint8_t>('a'));
  EXPECT_EQ(tx[3], static_cast<uint8_t>('b'));
  EXPECT_EQ(tx[4], static_cast<uint8_t>('c'));
  EXPECT_EQ(tx[5], static_cast<uint8_t>('d'));
  EXPECT_EQ(tx[6], 0x10);
  EXPECT_EQ(tx[7], 0x11);
  EXPECT_EQ(tx[8], 0x12);
  EXPECT_EQ(tx[9], 0x13);
  EXPECT_EQ(tx[10], 0xaa);
  EXPECT_EQ(tx[11], 0xbb);

  // Witness placeholder
  auto w = normalize::computeWitnessCommitmentPlaceholder(parts);
  for (auto b : w) EXPECT_EQ(b, 0u);
  parts.has_witness = true;
  parts.witness_commitment.fill(7);
  w = normalize::computeWitnessCommitmentPlaceholder(parts);
  for (auto b : w) EXPECT_EQ(b, 7u);
}


