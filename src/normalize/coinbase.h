#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace normalize {

struct CoinbaseParts {
  // Serialized coinbase transaction is assembled as:
  // prefix || extranonce1 || extranonce2 || suffix
  std::vector<uint8_t> prefix;  // includes version..scriptSig up to extranonce insertion point
  std::vector<uint8_t> suffix;  // remainder after extranonce insertion point through end of tx

  // Optional witness commitment (placeholder for now)
  bool has_witness{false};
  std::array<uint8_t, 32> witness_commitment{};  // if has_witness
};

// Assemble coinbase transaction bytes with given extranonce1/2.
std::vector<uint8_t> assembleCoinbaseTx(const CoinbaseParts& parts,
                                        std::string_view extranonce1,
                                        const std::vector<uint8_t>& extranonce2);

// Placeholder for witness commitment computation. For now, returns provided commitment if set,
// otherwise zeros.
std::array<uint8_t, 32> computeWitnessCommitmentPlaceholder(const CoinbaseParts& parts);

}  // namespace normalize


