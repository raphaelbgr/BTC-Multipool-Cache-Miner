#pragma once

#include <atomic>
#include <cstdint>
#include <string>
#include <thread>
#include <memory>
#include <mutex>

#include "adapters/stratum_adapter.h"

namespace net { class StratumClient; }

namespace adapters {

class StratumRunner {
 public:
  StratumRunner(StratumAdapter* adapter,
                std::string host, uint16_t port,
                std::string username, std::string password);
  ~StratumRunner();

  void start();
  void stop();

  // Submit a share for the last job. Returns false if not connected or no job.
  bool submitShare(const std::string& extranonce2_hex,
                   const std::string& ntime_hex,
                   const std::string& nonce_hex);

 private:
  void runLoop();

  StratumAdapter* adapter_;
  std::string host_;
  uint16_t port_;
  std::string username_;
  std::string password_;

  std::atomic<bool> stop_flag_{false};
  std::thread th_;

  std::mutex mu_;
  std::unique_ptr<net::StratumClient> client_;
  std::string last_job_id_;
  std::string extranonce1_;
  uint8_t extranonce2_size_{0};
};

}  // namespace adapters


