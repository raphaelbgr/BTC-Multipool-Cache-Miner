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
      std::string cookie_path; // optional override
  };
  std::optional<RpcConfig> rpc;

  // Optional GBT tuning
  struct GbtConfig {
    int poll_ms{2000};
    std::vector<std::string> rules; // e.g., ["segwit"]
    std::string cb_tag;             // coinbase tag string, informational
    bool allow_synth_coinbase{false}; // allow building minimal coinbase from default_witness_commitment when coinbasetxn missing
    std::string payout_script_hex;  // optional raw scriptPubKey (hex) for coinbase payout when coinbasetxn is missing
  };
  std::optional<GbtConfig> gbt;

    // Optional per-pool policy hints
    struct PolicyCfg {
      bool force_clean_jobs{false};
      bool clean_jobs_default{true};
      std::optional<uint32_t> version_mask;   // bitmask for version rolling
      std::optional<uint32_t> ntime_min;
      std::optional<uint32_t> ntime_max;
      std::optional<uint32_t> share_nbits;    // initial varDiff share target (compact)
    } policy;
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
    bool enable_http{false};
    std::string http_host{"127.0.0.1"};
    uint16_t http_port{8080};
    uint64_t file_max_bytes{10ull * 1024ull * 1024ull};
    uint64_t file_rotate_interval_sec{0};
  } metrics;

  // CUDA engine configuration
  struct CudaCfg {
    int hit_ring_capacity{1024};
    int desired_threads_per_job{256};
    int nonces_per_thread{1};
    int budget_ms{16};
  } cuda;

  // Outbox persistence configuration
  struct OutboxCfg {
    std::string path{"logs/outbox.bin"};
    uint64_t max_bytes{10ull * 1024ull * 1024ull}; // 10MB default
    bool rotate_on_start{false};
    uint64_t rotate_interval_sec{0}; // optional time-based rotation, 0=disabled
  } outbox;

  // Ledger persistence configuration
  struct LedgerCfg {
    std::string path{"runner.jsonl"};
    uint64_t max_bytes{10ull * 1024ull * 1024ull};
    uint64_t rotate_interval_sec{0};
  } ledger;
};

// Load from JSON file (e.g., config/pools.json). Returns empty config if file missing/invalid.
AppConfig loadFromJsonFile(const std::string& path);

  // Minimal default config used by header build tests.
  AppConfig loadDefault();

}


