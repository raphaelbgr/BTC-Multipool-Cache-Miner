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
  }

  // Update cached template: coinbase (if provided) and non-coinbase txs
  void updateTemplate(const nlohmann::json& gbt);

  // Submit a solved header (80B big-endian fields). Returns true on RPC accept.
  bool submitHeader(const uint8_t header80_be[80]);

 private:
  static std::string bytesToHex(const std::vector<uint8_t>& bytes);
  static std::string leHexFromHeaderBE(const uint8_t header80_be[80]);
  static std::string encodeVarIntHex(uint64_t v);

  net::RpcClient rpc_;
  // Cached pieces
  std::string coinbase_hex_;              // optional (may be empty)
  std::vector<std::string> txs_hex_;      // non-coinbase transactions
};

}  // namespace submit


