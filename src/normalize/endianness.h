#pragma once

#include <array>
#include <cstdint>
#include <cstring>

namespace normalize {

inline uint32_t bswap32(uint32_t v) {
  return ((v & 0x000000FFu) << 24) |
         ((v & 0x0000FF00u) << 8)  |
         ((v & 0x00FF0000u) >> 8)  |
         ((v & 0xFF000000u) >> 24);
}

inline uint64_t bswap64(uint64_t v) {
  return (static_cast<uint64_t>(bswap32(static_cast<uint32_t>(v))) << 32) |
         (static_cast<uint64_t>(bswap32(static_cast<uint32_t>(v >> 32))));
}

// Swap a 32-byte big-endian array to little-endian 8x u32 words.
inline std::array<uint32_t, 8> be32BytesToLeU32Words(const uint8_t be[32]) {
  std::array<uint32_t, 8> out{};
  for (int i = 0; i < 8; ++i) {
    const int o = i * 4;
    uint32_t w = (static_cast<uint32_t>(be[o]) << 24) |
                 (static_cast<uint32_t>(be[o + 1]) << 16) |
                 (static_cast<uint32_t>(be[o + 2]) << 8) |
                 (static_cast<uint32_t>(be[o + 3]));
    out[i] = bswap32(w);
  }
  return out;
}

// Convert u32 words (LE) to a 32-byte big-endian array.
inline void leU32WordsToBe32Bytes(const std::array<uint32_t, 8>& words, uint8_t be[32]) {
  for (int i = 0; i < 8; ++i) {
    const uint32_t w = bswap32(words[i]);
    const int o = i * 4;
    be[o]     = static_cast<uint8_t>((w >> 24) & 0xFF);
    be[o + 1] = static_cast<uint8_t>((w >> 16) & 0xFF);
    be[o + 2] = static_cast<uint8_t>((w >> 8) & 0xFF);
    be[o + 3] = static_cast<uint8_t>(w & 0xFF);
  }
}

}  // namespace normalize


