#pragma once

#include <cstdint>
#include <vector>

namespace normalize {

// Compute Bitcoin-style Merkle root from a list of TXIDs (each 32-byte big-endian).
// If the number of items at a level is odd, the last item is duplicated.
// Returns 32-byte big-endian merkle root. For empty list, returns 32 zero bytes.
void compute_merkle_root_be(const std::vector<std::array<uint8_t, 32>>& txids_be,
                            uint8_t out32_be[32]);

}


