#pragma once

#include <array>
#include <cstdint>

namespace normalize {

// Convert compact nBits (Bitcoin) to LE u32[8] target.
// Handles edge cases similar to Bitcoin Core (zero/overflow clamps).
std::array<uint32_t, 8> compactToTargetU32LE(uint32_t nBits);

}


