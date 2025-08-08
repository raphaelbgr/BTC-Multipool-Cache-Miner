#include "normalize/targets.h"

#include <algorithm>
#include <cstring>
#include <cstdint>

namespace normalize {

static void setZero(std::array<uint32_t, 8>& t) {
  for (auto& w : t) w = 0u;
}

static void setFromBigEndianBytes(std::array<uint32_t, 8>& out, const uint8_t be[32]) {
  // Convert 32-byte big-endian to LE u32 words
  for (int i = 0; i < 8; ++i) {
    const int o = i * 4;
    uint32_t w = (static_cast<uint32_t>(be[o]) << 24) |
                 (static_cast<uint32_t>(be[o + 1]) << 16) |
                 (static_cast<uint32_t>(be[o + 2]) << 8) |
                 (static_cast<uint32_t>(be[o + 3]));
    // store as LE word
    out[i] = ((w & 0x000000FFu) << 24) |
             ((w & 0x0000FF00u) << 8)  |
             ((w & 0x00FF0000u) >> 8)  |
             ((w & 0xFF000000u) >> 24);
  }
}

std::array<uint32_t, 8> compactToTargetU32LE(uint32_t nBits) {
  std::array<uint32_t, 8> target{};
  setZero(target);

  uint32_t exponent = nBits >> 24;
  uint32_t mantissa = nBits & 0x007fffffU;

  // Negative target (sign bit) not expected; treat as zero.
  if (nBits & 0x00800000U) {
    return target;
  }

  // Zero mantissa yields zero target.
  if (mantissa == 0) {
    return target;
  }

  // Construct big-endian 32-byte array per Bitcoin encoding.
  uint8_t be[32] = {0};
  int bytes = static_cast<int>(exponent);
  if (bytes <= 3) {
    uint32_t m = mantissa >> (8 * (3 - bytes));
    be[32 - bytes] = static_cast<uint8_t>((m >> 16) & 0xFF);
    if (bytes >= 2) be[32 - bytes + 1] = static_cast<uint8_t>((m >> 8) & 0xFF);
    if (bytes >= 3) be[32 - bytes + 2] = static_cast<uint8_t>(m & 0xFF);
  } else {
    int idx = 32 - bytes;
    if (idx < 0) {
      // Overflow: exponent beyond 32 bytes -> out of range; return max (all 0xFF)
      for (auto& w : target) w = 0xFFFFFFFFu;
      return target;
    }
    be[idx]     = static_cast<uint8_t>((mantissa >> 16) & 0xFF);
    be[idx + 1] = static_cast<uint8_t>((mantissa >> 8) & 0xFF);
    be[idx + 2] = static_cast<uint8_t>(mantissa & 0xFF);
  }

  setFromBigEndianBytes(target, be);
  return target;
}

}  // namespace normalize


