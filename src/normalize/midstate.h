#pragma once

#include <array>
#include <cstdint>

namespace normalize {

// SHA-256 midstate after processing exactly 64 bytes.
// Returns internal state words (eight 32-bit words).
std::array<uint32_t, 8> sha256_midstate_64(const uint8_t first64[64]);

// Finish SHA-256 from a given midstate, processing the remaining tail bytes
// so that the total message length equals total_len_bytes.
// Produces the final 32-byte hash (big-endian) into out32.
void sha256_finish_from_midstate(const std::array<uint32_t, 8>& midstate,
                                 const uint8_t* tail, size_t tail_len,
                                 uint64_t total_len_bytes,
                                 uint8_t out32[32]);

}


