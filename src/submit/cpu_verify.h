#pragma once

#include <array>
#include <cstdint>
#include <vector>

namespace submit {

// SHA-256 (single) and double-SHA256 utilities on CPU.
void sha256(const uint8_t* data, size_t len, uint8_t out32[32]);
void sha256d(const uint8_t* data, size_t len, uint8_t out32[32]);

// Compare a 32-byte hash (big-endian) against a target (LE u32[8]);
// Returns true if hash <= target.
bool isHashLEQTarget(const uint8_t hash_be[32], const std::array<uint32_t, 8>& target_le);

}  // namespace submit


