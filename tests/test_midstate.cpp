#include <gtest/gtest.h>
#include "normalize/midstate.h"
#include "submit/cpu_verify.h"

TEST(Midstate, MatchesFullHash_80bytes) {
  // Construct a fake 80-byte header (all zeros except nonce)
  uint8_t header[80] = {};
  header[76] = 0x12; header[77] = 0x34; header[78] = 0x56; header[79] = 0x78; // nonce

  uint8_t full[32];
  submit::sha256(header, 80, full); // first round only

  auto mid = normalize::sha256_midstate_64(header);
  uint8_t tail_hash[32];
  normalize::sha256_finish_from_midstate(mid, header + 64, 16, 80, tail_hash);

  // The midstate-finished hash should equal the full first-round sha256
  for (int i = 0; i < 32; ++i) {
    EXPECT_EQ(full[i], tail_hash[i]);
  }
}


