#include <gtest/gtest.h>
#include <vector>
#include "normalize/merkle.h"
#include "submit/submit_router.h"
#include "normalize/targets.h"

TEST(Merkle, EmptyIsZero) {
  uint8_t root[32];
  normalize::compute_merkle_root_be({}, root);
  for (int i = 0; i < 32; ++i) EXPECT_EQ(root[i], 0);
}

TEST(Merkle, TwoLeaves) {
  std::array<uint8_t, 32> a{}; a[31] = 1;
  std::array<uint8_t, 32> b{}; b[31] = 2;
  uint8_t root[32];
  normalize::compute_merkle_root_be({a, b}, root);
  // Not checking exact value, just stable behavior: re-run should match
  uint8_t root2[32];
  normalize::compute_merkle_root_be({a, b}, root2);
  for (int i = 0; i < 32; ++i) EXPECT_EQ(root[i], root2[i]);
}

TEST(SubmitRouter, VerifyAndDispatch) {
  // Build an 80-byte header and use a maximal target so any hash satisfies target
  uint8_t header[80] = {};
  std::array<uint32_t, 8> target{};
  for (auto &w : target) w = 0xFFFFFFFFu;

  bool called = false;
  submit::SubmitRouter router([&](const submit::HitRecord& rec){ called = true; (void)rec; });
  bool ok = router.verifyAndSubmit(header, target, 7, 0);
  EXPECT_TRUE(ok);
  EXPECT_TRUE(called);
}


