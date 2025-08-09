#include "normalize/coinbase.h"

#include <algorithm>

namespace normalize {

std::vector<uint8_t> assembleCoinbaseTx(const CoinbaseParts& parts,
                                        std::string_view extranonce1,
                                        const std::vector<uint8_t>& extranonce2) {
  std::vector<uint8_t> tx;
  tx.reserve(parts.prefix.size() + extranonce1.size() + extranonce2.size() + parts.suffix.size());
  tx.insert(tx.end(), parts.prefix.begin(), parts.prefix.end());
  tx.insert(tx.end(), reinterpret_cast<const uint8_t*>(extranonce1.data()),
            reinterpret_cast<const uint8_t*>(extranonce1.data()) + extranonce1.size());
  tx.insert(tx.end(), extranonce2.begin(), extranonce2.end());
  tx.insert(tx.end(), parts.suffix.begin(), parts.suffix.end());
  return tx;
}

std::array<uint8_t, 32> computeWitnessCommitmentPlaceholder(const CoinbaseParts& parts) {
  if (parts.has_witness) return parts.witness_commitment;
  std::array<uint8_t, 32> z{};
  z.fill(0);
  return z;
}

}  // namespace normalize


