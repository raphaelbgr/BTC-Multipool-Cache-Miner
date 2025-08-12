#include "submit/gbt_submitter.h"

#include <cstdio>
#include <sstream>

namespace submit {

void GbtSubmitter::updateTemplate(const nlohmann::json& gbt) {
  // Reset
  coinbase_hex_.clear();
  txs_hex_.clear();
  try {
    if (gbt.contains("coinbasetxn") && gbt["coinbasetxn"].is_object()) {
      coinbase_hex_ = gbt["coinbasetxn"]["data"].get<std::string>();
    }
    if (gbt.contains("transactions") && gbt["transactions"].is_array()) {
      for (const auto& tx : gbt["transactions"]) {
        if (tx.contains("data")) txs_hex_.push_back(tx["data"].get<std::string>());
      }
    }
  } catch (...) {
    // ignore malformed
  }
}

std::string GbtSubmitter::bytesToHex(const std::vector<uint8_t>& bytes) {
  static const char* kHex = "0123456789abcdef";
  std::string out; out.resize(bytes.size() * 2);
  for (size_t i = 0; i < bytes.size(); ++i) {
    out[2*i] = kHex[(bytes[i] >> 4) & 0xF];
    out[2*i+1] = kHex[bytes[i] & 0xF];
  }
  return out;
}

std::string GbtSubmitter::encodeVarIntHex(uint64_t v) {
  std::ostringstream oss;
  auto emitLE = [&](uint64_t vv, int bytes){
    static const char* kHex = "0123456789abcdef";
    for (int i = 0; i < bytes; ++i) {
      uint8_t b = static_cast<uint8_t>((vv >> (8*i)) & 0xFF);
      oss << kHex[(b >> 4) & 0xF] << kHex[b & 0xF];
    }
  };
  if (v < 0xfd) {
    emitLE(v, 1);
  } else if (v <= 0xffff) {
    oss << "fd"; emitLE(v, 2);
  } else if (v <= 0xffffffffULL) {
    oss << "fe"; emitLE(v, 4);
  } else {
    oss << "ff"; emitLE(v, 8);
  }
  return oss.str();
}

std::string GbtSubmitter::leHexFromHeaderBE(const uint8_t header80_be[80]) {
  // Flip 80-byte header from BE fields to LE hex as expected by submitblock + coinbase+txs concatenation.
  // This follows cgminer style: hex string is little-endian of the header bytes.
  std::string s; s.reserve(160);
  static const char* kHex = "0123456789abcdef";
  for (int i = 0; i < 80; ++i) {
    uint8_t b = header80_be[i];
    s.push_back(kHex[(b >> 4) & 0xF]);
    s.push_back(kHex[b & 0xF]);
  }
  return s;
}

bool GbtSubmitter::submitHeader(const uint8_t header80_be[80]) {
  // Build block hex: header + varint(tx_count) + [coinbase + txs]
  std::string block_hex = leHexFromHeaderBE(header80_be);
  const uint64_t tx_count = (coinbase_hex_.empty() ? 0 : 1) + txs_hex_.size();
  block_hex += encodeVarIntHex(tx_count);
  if (!coinbase_hex_.empty()) block_hex += coinbase_hex_;
  for (const auto& tx : txs_hex_) block_hex += tx;
  nlohmann::json params = nlohmann::json::array();
  params.push_back(block_hex);
  auto res = rpc_.call("submitblock", params);
  return res.has_value();
}

} // namespace submit


