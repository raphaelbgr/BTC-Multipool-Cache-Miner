#pragma once

#include <cstdint>
#include <optional>
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
  int weight{1};        // scheduler weight hint
  // Credentials (one of the following used based on cred_mode)
  std::string wallet;   // for wallet_as_user
  std::string account;  // for account_worker
  std::string worker;   // optional suffix
  std::string password; // x/123, etc.
  std::vector<Endpoint> endpoints;
  // Optional Bitcoin Core RPC (for GBT)
  struct RpcConfig {
    std::string url;     // e.g., http://127.0.0.1:8332
    bool use_tls{false};
    std::string auth;    // "cookie" or "userpass"
    std::string username;
    std::string password;
  };
  std::optional<RpcConfig> rpc;

  // Optional GBT tuning
  struct GbtConfig {
    int poll_ms{2000};
    std::vector<std::string> rules; // e.g., ["segwit"]
    std::string cb_tag;             // coinbase tag string, informational
  };
  std::optional<GbtConfig> gbt;
};

struct AppConfig {
  int log_level{2}; // 0-trace,1-debug,2-info,3-warn,4-error
  std::vector<PoolEntry> pools;
  // Scheduler tuning
  struct SchedulerCfg {
    int latency_penalty_ms{1500}; // apply penalty if avg submit latency exceeds this
    int max_weight{4};            // cap per-source weight replication
  } scheduler;
  // Metrics sink configuration
  struct MetricsCfg {
    bool enable_file{false};
    std::string file_path{"logs/metrics.jsonl"};
    int dump_interval_ms{2000};
  } metrics;

  // CUDA engine configuration
  struct CudaCfg {
    int hit_ring_capacity{1024};
    int desired_threads_per_job{256};
  } cuda;
};

// Load from JSON file (e.g., config/pools.json). Returns empty config if file missing/invalid.
AppConfig loadFromJsonFile(const std::string& path);

  // Minimal default config used by header build tests.
  AppConfig loadDefault();

}


