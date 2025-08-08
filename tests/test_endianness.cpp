#include <gtest/gtest.h>
#include "normalize/endianness.h"

TEST(Endianness, Bswap32) {
  EXPECT_EQ(normalize::bswap32(0xAABBCCDDu), 0xDDCCBBAAu);
  EXPECT_EQ(normalize::bswap32(0x00000000u), 0x00000000u);
}

TEST(Endianness, Bswap64) {
  EXPECT_EQ(normalize::bswap64(0x1122334455667788ull), 0x8877665544332211ull);
}

TEST(Endianness, ArrayRoundTrip) {
  uint8_t be[32] = {};
  for (int i = 0; i < 32; ++i) be[i] = static_cast<uint8_t>(i);
  auto words = normalize::be32BytesToLeU32Words(be);
  uint8_t roundtrip[32] = {};
  normalize::leU32WordsToBe32Bytes(words, roundtrip);
  for (int i = 0; i < 32; ++i) {
    EXPECT_EQ(roundtrip[i], be[i]);
  }
}


