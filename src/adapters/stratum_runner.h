#pragma once

#include <atomic>
#include <cstdint>
#include <string>
#include <thread>
#include <memory>
#include <mutex>
#include <chrono>
#include <functional>

#include "adapters/stratum_adapter.h"

namespace net { class StratumClient; }

namespace adapters {

class StratumRunner {
 public:
  StratumRunner(StratumAdapter* adapter,
                std::string host, uint16_t port,
                std::string username, std::string password,
                bool use_tls = false);
  ~StratumRunner();

  void start();
  void stop();

  // Submit a share for the last job. Returns false if not connected or no job.
  bool submitShare(const std::string& extranonce2_hex,
                   const std::string& ntime_hex,
                   const std::string& nonce_hex);

  // Record mapping for the next submit so we can report it on acceptance
  void setPendingSubmit(uint64_t work_id, uint32_t nonce) {
    std::lock_guard<std::mutex> lk(mu_);
    pending_work_id_ = work_id;
    pending_nonce_ = nonce;
  }

  int connectFailures() const { return consecutive_connect_failures_.load(); }
  int quickDisconnects() const { return consecutive_quick_disconnects_.load(); }
  void resetCounters() { consecutive_connect_failures_.store(0); consecutive_quick_disconnects_.store(0); }

  int acceptedSubmits() const { return accepted_submits_.load(); }
  int rejectedSubmits() const { return rejected_submits_.load(); }
  int avgSubmitMs() const { return avg_submit_ms_.load(); }

  // Callback hook registration for accepted submissions
  using AcceptedCallback = std::function<void(uint64_t work_id, uint32_t nonce)>;
  void setAcceptedCallback(AcceptedCallback cb) { on_accepted_ = std::move(cb); }

 private:
  void runLoop();

  StratumAdapter* adapter_;
  std::string host_;
  uint16_t port_;
  std::string username_;
  std::string password_;
  bool use_tls_{false};

  std::atomic<bool> stop_flag_{false};
  std::thread th_;

  std::mutex mu_;
  std::unique_ptr<net::StratumClient> client_;
  std::string last_job_id_;
  std::string extranonce1_;
  uint8_t extranonce2_size_{0};

  std::atomic<int> consecutive_connect_failures_{0};
  std::atomic<int> consecutive_quick_disconnects_{0};
  std::atomic<int> accepted_submits_{0};
  std::atomic<int> rejected_submits_{0};
  std::atomic<int> avg_submit_ms_{0};
  std::chrono::steady_clock::time_point last_submit_sent_{};
  AcceptedCallback on_accepted_{};
  uint64_t pending_work_id_{0};
  uint32_t pending_nonce_{0};
};

}  // namespace adapters


