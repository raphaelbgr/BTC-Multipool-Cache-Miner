#include "adapters/stratum_runner.h"

#include <chrono>
#include <iostream>
#include "obs/log.h"

#include <nlohmann/json.hpp>

#include "net/stratum_client.h"
#include "normalize/normalizer.h"
#include "normalize/endianness.h"

namespace adapters {

StratumRunner::StratumRunner(StratumAdapter* adapter,
                             std::string host, uint16_t port,
                             std::string username, std::string password,
                             bool use_tls)
    : adapter_(adapter), host_(std::move(host)), port_(port),
      username_(std::move(username)), password_(std::move(password)), use_tls_(use_tls) {}

StratumRunner::~StratumRunner() { stop(); }

void StratumRunner::start() {
  if (th_.joinable()) return;
  stop_flag_.store(false);
  th_ = std::thread(&StratumRunner::runLoop, this);
}

void StratumRunner::stop() {
  stop_flag_.store(true);
  if (th_.joinable()) th_.join();
}

static uint32_t compactFromDifficulty(double diff) {
  // Very rough placeholder: typical pools send compact for block, difficulty for shares.
  // Use Bitcoin mainnet baseline: diff 1 => target 0x1d00ffff / 2^32 scaling.
  // For unit testing/integration, adapter policy will be updated with share_nbits.
  (void)diff;
  return 0x1f00ffffu; // easy share target placeholder
}

void StratumRunner::runLoop() {
  obs::Logger log("stratum");
  client_ = std::make_unique<net::StratumClient>(host_, port_, username_, password_, use_tls_);
  int attempt = 0;
  while (!stop_flag_.load()) {
    log.info("connecting", {{"host", host_}, {"port", port_}, {"attempt", attempt + 1}});
    if (!client_->connect()) {
      int backoff_s = std::min(30, 1 << std::min(attempt, 4)); // 1,2,4,8,16, then cap
      log.warn("connect_failed", {{"host", host_}, {"port", port_}, {"backoff_s", backoff_s}});
      consecutive_connect_failures_.fetch_add(1);
      std::this_thread::sleep_for(std::chrono::seconds(backoff_s));
      attempt++;
      continue;
    }
    consecutive_connect_failures_.store(0);
    attempt = 0;
    log.info("connected", {{"host", host_}, {"port", port_}});
    client_->sendSubscribe();
    client_->sendAuthorize();
    // Negotiate version rolling if supported
    try {
      nlohmann::json cfg;
      cfg["id"] = 100;
      cfg["method"] = "mining.configure";
      cfg["params"] = nlohmann::json::array();
      cfg["params"].push_back(nlohmann::json::array({"version-rolling"}));
      cfg["params"].push_back(nlohmann::json{{"version-rolling.mask", "ffffffff"}});
      client_->sendJson(cfg);
    } catch (...) {}
    adapter_->connect();

    // If a server drops immediately after TCP connect (e.g., proxy), loop will reconnect.
    while (!stop_flag_.load()) {
      auto [status, line] = client_->recvLine();
      if (status == net::StratumClient::RecvStatus::kTimeout) {
        // Periodic heartbeat; continue waiting for data
        continue;
      }
      if (status == net::StratumClient::RecvStatus::kClosed) break;
      nlohmann::json j = nlohmann::json::parse(line, nullptr, false);
      if (j.is_discarded()) continue;
      if (j.contains("result") && j["id"] == 1) {
        // subscribe result: [ [ [sub_details], extranonce1, extranonce2_size ], session_id ]
        try {
          std::lock_guard<std::mutex> lk(mu_);
          extranonce1_ = j["result"][1].get<std::string>();
          extranonce2_size_ = static_cast<uint8_t>(j["result"][2].get<int>());
          adapter_->onSubscribed(extranonce1_, extranonce2_size_);
          log.info("subscribed", {{"extranonce1", extranonce1_}, {"extranonce2_size", static_cast<int>(extranonce2_size_)}});
        } catch (...) {}
        continue;
      }
      if (j.contains("result") && j["id"] == 2) {
        try {
          bool ok = j["result"].get<bool>();
          log.info("authorize", {{"ok", ok}});
        } catch (...) {}
        continue;
      }
      if (j.contains("result") && j["id"] == 100) {
        // mining.configure response
        try {
          auto res = j["result"];
          if (res.contains("version-rolling") && res["version-rolling"].get<bool>()) {
            std::string mask_hex = res.value<std::string>("version-rolling.mask", "00000000");
            uint32_t mask = 0;
            try { mask = std::stoul(mask_hex, nullptr, 16); } catch (...) { mask = 0; }
            adapter_->setVersionMask(mask);
            log.info("version_rolling", {{"mask", mask_hex}});
          }
        } catch (...) {}
        continue;
      }
      if (j.contains("id") && j["id"] == 3) {
        // mining.submit response
        try {
          if (j.contains("result")) {
            bool ok = j["result"].get<bool>();
            log.info("submit_result", {{"accepted", ok}});
            if (ok) accepted_submits_.fetch_add(1); else rejected_submits_.fetch_add(1);
          } else if (j.contains("error") && !j["error"].is_null()) {
            // error format: [code, message, data]
            auto err = j["error"];
            int code = 0; std::string msg;
            try { code = err[0].get<int>(); } catch (...) {}
            try { msg = err[1].get<std::string>(); } catch (...) {}
            log.warn("submit_error", {{"code", code}, {"message", msg}});
            rejected_submits_.fetch_add(1);
          }
        } catch (...) {}
        continue;
      }
      if (j.contains("method")) {
        std::string m = j["method"].get<std::string>();
        if (m == "mining.set_difficulty") {
          try {
            double diff = j["params"][0].get<double>();
            uint32_t share_nbits = compactFromDifficulty(diff);
            adapter_->updateVarDiff(share_nbits);
            log.info("set_difficulty", {{"diff", diff}, {"share_nbits_hex", share_nbits}});
          } catch (...) {}
        } else if (m == "mining.notify") {
          try {
            auto params = j["params"];
            normalize::RawJobInputs in;
            in.source_id = 0;
            in.job.source_name = "stratum";
            std::string job_id = params[0].get<std::string>();
            {
              std::lock_guard<std::mutex> lk(mu_);
              last_job_id_ = job_id;
            }
            in.job.work_id = static_cast<uint64_t>(std::hash<std::string>{}(job_id));
            in.job.version = std::stoul(params[8].get<std::string>(), nullptr, 16);
            in.job.nbits = std::stoul(params[9].get<std::string>(), nullptr, 16);
            in.job.ntime = std::stoul(params[10].get<std::string>(), nullptr, 16);
            // prevhash, merkle_root from notify params
            auto prevhash_hex = params[1].get<std::string>();
            auto mr_hex = params[2].get<std::string>();
            // fill BE bytes from hex (minimal, no validation)
            auto hexToBytes = [](const std::string& hex, uint8_t* out, size_t out_len){
              size_t n = std::min(hex.size()/2, out_len);
              for (size_t i=0;i<n;i++){ unsigned int v; sscanf(hex.c_str()+2*i, "%02x", &v); out[i]=static_cast<uint8_t>(v);} };
            hexToBytes(prevhash_hex, in.prevhash_be, 32);
            hexToBytes(mr_hex, in.merkle_root_be, 32);
            // clean_jobs flag (param index 7 per Stratum V1)
            bool clean = false;
            try { clean = params[7].get<bool>(); } catch (...) { clean = false; }
            in.clean_jobs = clean;
            // propagate known extranonce2 size from subscription
            {
              std::lock_guard<std::mutex> lk(mu_);
              in.extranonce2_size = extranonce2_size_;
            }
            // Update adapter policy clean_jobs default to reflect server intent without forcing it
            adapter_->setCleanJobs(clean);
            // Apply basic ntime caps around provided ntime to allow minimal rolling
            in.ntime_min = in.job.ntime;
            in.ntime_max = in.job.ntime + 2;
            // Ensure share target is set from current varDiff policy if present
            // (Stratum set_difficulty event updates adapter policy already)
            // header_first64 for midstate: version..merkle_root (64 bytes)
            // Here we leave zeros as placeholder; midstate still computed over zeros.
            adapter_->ingestJobWithPolicy(in);
            auto shortHex = [](const std::string& h){ return h.size()>16 ? h.substr(0,16) : h; };
            log.info("notify", {{"job_id", job_id},
                                  {"version_hex", in.job.version},
                                  {"nbits_hex", in.job.nbits},
                                  {"ntime_hex", in.job.ntime},
                                  {"prevhash_prefix", shortHex(prevhash_hex)},
                                  {"merkle_prefix", shortHex(mr_hex)}});
          } catch (...) {}
        }
      }
    }
    client_->close();
    log.warn("disconnected_retry", {{"host", host_}, {"port", port_}});
    consecutive_quick_disconnects_.fetch_add(1);
    std::this_thread::sleep_for(std::chrono::seconds(2));
  }
}

bool StratumRunner::submitShare(const std::string& extranonce2_hex,
                                const std::string& ntime_hex,
                                const std::string& nonce_hex) {
  std::lock_guard<std::mutex> lk(mu_);
  if (!client_) return false;
  if (last_job_id_.empty()) return false;
  return client_->sendSubmit(username_, last_job_id_, extranonce2_hex, ntime_hex, nonce_hex);
}

}  // namespace adapters


