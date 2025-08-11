#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace config {

enum class CredMode { WalletAsUser, AccountWorker };

struct Endpoint {
  std::string host;
  uint16_t port{0};
  bool use_tls{false};
};

struct PoolEntry {
  std::string name;
  std::string profile;  // viabtc, f2pool, ckpool, etc.
  CredMode cred_mode{CredMode::WalletAsUser};
  // Credentials (one of the following used based on cred_mode)
  std::string wallet;   // for wallet_as_user
  std::string account;  // for account_worker
  std::string worker;   // optional suffix
  std::string password; // x/123, etc.
  std::vector<Endpoint> endpoints;
};

struct AppConfig {
  int log_level{2}; // 0-trace,1-debug,2-info,3-warn,4-error
  std::vector<PoolEntry> pools;
};

// Load from JSON file (e.g., config/pools.json). Returns empty config if file missing/invalid.
AppConfig loadFromJsonFile(const std::string& path);

}


