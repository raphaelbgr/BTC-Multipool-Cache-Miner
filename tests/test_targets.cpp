#include <gtest/gtest.h>
#include <array>
#include <cstdint>
#include "normalize/targets.h"

static std::array<uint32_t, 8> beHexToLeWords(const char* hex) {
  // hex length 64
  std::array<uint8_t, 32> be{};
  for (int i = 0; i < 32; ++i) {
    char h1 = hex[i * 2];
    char h2 = hex[i * 2 + 1];
    auto nyb = [](char c) -> uint8_t {
      if (c >= '0' && c <= '9') return c - '0';
      if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
      if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
      return 0;
    };
    be[i] = static_cast<uint8_t>((nyb(h1) << 4) | nyb(h2));
  }
  std::array<uint32_t, 8> out{};
  for (int i = 0; i < 8; ++i) {
    int o = i * 4;
    uint32_t w = (static_cast<uint32_t>(be[o]) << 24) |
                 (static_cast<uint32_t>(be[o + 1]) << 16) |
                 (static_cast<uint32_t>(be[o + 2]) << 8) |
                 (static_cast<uint32_t>(be[o + 3]));
    // convert BE to LE word ordering
    uint32_t le = ((w & 0x000000FFu) << 24) |
                  ((w & 0x0000FF00u) << 8) |
                  ((w & 0x00FF0000u) >> 8) |
                  ((w & 0xFF000000u) >> 24);
    out[i] = le;
  }
  return out;
}

TEST(Targets, GenesisVector) {
  uint32_t nBits = 0x1d00ffffu; // Bitcoin genesis
  auto t = normalize::compactToTargetU32LE(nBits);
  auto expected = beHexToLeWords("00000000ffff0000000000000000000000000000000000000000000000000000");
  for (int i = 0; i < 8; ++i) {
    EXPECT_EQ(t[i], expected[i]);
  }
}

TEST(Targets, ZeroMantissa) {
  auto t = normalize::compactToTargetU32LE(0x1d000000u);
  for (auto w : t) EXPECT_EQ(w, 0u);
}


