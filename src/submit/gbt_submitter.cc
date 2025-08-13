#include "submit/gbt_submitter.h"

#include <cstdio>
#include <sstream>
#include <iomanip>

namespace submit {

void GbtSubmitter::updateTemplate(const nlohmann::json& gbt) {
  // Reset
  coinbase_hex_.clear();
  txs_hex_.clear();
  witness_commitment_hex_.reset();
  try {
    if (gbt.contains("coinbasetxn") && gbt["coinbasetxn"].is_object()) {
      coinbase_hex_ = gbt["coinbasetxn"]["data"].get<std::string>();
    }
    if (gbt.contains("transactions") && gbt["transactions"].is_array()) {
      for (const auto& tx : gbt["transactions"]) {
        if (tx.contains("data")) txs_hex_.push_back(tx["data"].get<std::string>());
      }
    }
    if (gbt.contains("default_witness_commitment") && gbt["default_witness_commitment"].is_string()) {
      witness_commitment_hex_ = gbt["default_witness_commitment"].get<std::string>();
      // If no coinbasetxn provided, build a minimal coinbase to carry witness commitment for diagnostic runs
      if (coinbase_hex_.empty()) {
        // Height is optional; if available in GBT use it, else use 0
        uint32_t height = 0;
        try { if (gbt.contains("height")) height = static_cast<uint32_t>(gbt["height"].get<int>()); } catch(...) {}
        std::string c;
        if (buildMinimalWitnessCommitmentCoinbase(height, *witness_commitment_hex_, &c)) {
          coinbase_hex_ = std::move(c);
        }
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
  std::string block_hex;
  if (!buildBlockHex(header80_be, &block_hex)) return false;
  nlohmann::json params = nlohmann::json::array();
  params.push_back(block_hex);
  auto res = rpc_.call("submitblock", params);
  return res.has_value();
}

bool GbtSubmitter::buildBlockHex(const uint8_t header80_be[80], std::string* out_hex) {
  if (!out_hex) return false;
  // Build block hex: header + varint(tx_count) + [coinbase + txs]
  std::string block_hex = leHexFromHeaderBE(header80_be);
  const uint64_t tx_count = (coinbase_hex_.empty() ? 0 : 1) + txs_hex_.size();
  block_hex += encodeVarIntHex(tx_count);
  if (!coinbase_hex_.empty()) block_hex += coinbase_hex_;
  for (const auto& tx : txs_hex_) block_hex += tx;
  *out_hex = std::move(block_hex);
  return true;
}

// Encode a compact int (Bitcoin varint) to hex appended to oss (little-endian inside helper above covers values >= 0xfd).
static void appendVarInt(std::ostringstream& oss, uint64_t v) {
  oss << GbtSubmitter::encodeVarIntHex(v);
}

// Convert uint32 height to BIP34 little-endian script push (compact int + LE bytes sans leading zeros)
static std::string encodeBip34Height(uint32_t height) {
  // strip leading zero bytes
  std::vector<uint8_t> le;
  while (height) { le.push_back(static_cast<uint8_t>(height & 0xFF)); height >>= 8; }
  if (le.empty()) le.push_back(0);
  std::ostringstream oss;
  // push data opcode: single-byte length
  oss << std::hex << std::setfill('0');
  oss << std::setw(2) << static_cast<int>(le.size());
  for (uint8_t b : le) { oss << std::setw(2) << static_cast<int>(b); }
  return oss.str();
}

bool GbtSubmitter::buildMinimalWitnessCommitmentCoinbase(uint32_t height_le, const std::string& commitment32_hex, std::string* out_tx_hex) {
  if (!out_tx_hex) return false;
  if (commitment32_hex.size() != 64) return false; // 32 bytes hex
  std::ostringstream oss; oss << std::hex << std::setfill('0');
  // version
  oss << "01000000";
  // txin count
  appendVarInt(oss, 1);
  // prevout (32 bytes zero + 0xffffffff)
  for (int i=0;i<32;++i) oss << "00";
  oss << "ffffffff";
  // scriptsig: BIP34 height only
  std::string h = encodeBip34Height(height_le);
  appendVarInt(oss, h.size()/2);
  oss << h;
  // sequence
  oss << "ffffffff";
  // txout count: 1 (OP_RETURN witness commitment)
  appendVarInt(oss, 1);
  // value: 0 (8 bytes little-endian)
  oss << "0000000000000000";
  // scriptPubKey: OP_RETURN (0x6a) + push(36=0x24) + 0xaa21a9ed + 32-byte commitment
  std::string spk;
  spk.reserve(2 + 2 + 8 + 64);
  spk += "6a"; // OP_RETURN
  spk += "24"; // push 36 bytes
  spk += "aa21a9ed"; // witness nonce tag
  spk += commitment32_hex; // 32 bytes commitment
  appendVarInt(oss, spk.size()/2);
  oss << spk;
  // locktime
  oss << "00000000";
  *out_tx_hex = oss.str();
  return true;
}

} // namespace submit


