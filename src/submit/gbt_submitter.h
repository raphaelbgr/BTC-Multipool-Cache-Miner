#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "config/config.h"
#include "net/rpc_client.h"

namespace submit {

// Builds full block from header + cached GBT template transactions and submits via submitblock.
class GbtSubmitter {
 public:
  explicit GbtSubmitter(const config::PoolEntry::RpcConfig& rpc_cfg)
      : rpc_(rpc_cfg.url) {
    if (rpc_cfg.auth == "userpass") {
      rpc_.setBasicAuth(rpc_cfg.username, rpc_cfg.password);
    }
    if (rpc_cfg.auth == "cookie" && !rpc_cfg.cookie_path.empty()) {
      rpc_.setCookieAuthFile(rpc_cfg.cookie_path);
    }
  }

  void setAllowSynthCoinbase(bool allow) { allow_synth_coinbase_ = allow; }
  void setPayoutScriptHex(std::string hex) { payout_script_hex_ = std::move(hex); }
  void setCoinbaseTag(std::string tag) { coinbase_tag_ = std::move(tag); }

  bool hasCoinbase() const { return !coinbase_hex_.empty(); }

  // Update cached template: coinbase (if provided) and non-coinbase txs
  void updateTemplate(const nlohmann::json& gbt);

  // Submit a solved header (80B big-endian fields). Returns true on RPC accept.
  bool submitHeader(const uint8_t header80_be[80]);

  // Build full block hex without submitting. Returns false if template incomplete (e.g., missing coinbase).
  bool buildBlockHex(const uint8_t header80_be[80], std::string* out_hex);

  // Helper: build a minimal coinbase tx hex that carries a witness commitment OP_RETURN for testing/diagnostics.
  // Not consensus-complete (omits spendable output). Height is encoded per BIP34 in scriptsig.
  static bool buildMinimalWitnessCommitmentCoinbase(uint32_t height_le, const std::string& commitment32_hex, std::string* out_tx_hex);

  // Helper: build a full coinbase tx hex with a spendable payout output of value_sat and an OP_RETURN witness commitment output.
  static bool buildCoinbaseTx(uint32_t height_le,
                              const std::string& payout_script_hex,
                              uint64_t value_sat,
                              const std::string& commitment32_hex,
                              const std::string& extra_sig_push_hex,
                              std::string* out_tx_hex);

 private:
  static std::string bytesToHex(const std::vector<uint8_t>& bytes);
  static std::string leHexFromHeaderBE(const uint8_t header80_be[80]);
  
  // Exposed for helpers implemented outside the class
 public:
  static std::string encodeVarIntHex(uint64_t v);
 private:

  net::RpcClient rpc_;
  // Cached pieces
  std::string coinbase_hex_;              // optional (may be empty)
  std::vector<std::string> txs_hex_;      // non-coinbase transactions
  std::optional<std::string> witness_commitment_hex_; // from default_witness_commitment
  bool allow_synth_coinbase_{false};
  std::string payout_script_hex_;
  std::string coinbase_tag_;
};

}  // namespace submit


