#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <nlohmann/json.hpp>

#include "adapters/gbt_adapter.h"
#include "config/config.h"
#include "submit/gbt_submitter.h"

namespace adapters {

// Periodically polls getblocktemplate and feeds normalized jobs into GbtAdapter.
class GbtRunner {
 public:
  GbtRunner(GbtAdapter* adapter, const config::PoolEntry& pool_cfg);
  ~GbtRunner();

  void start();
  void stop();

 private:
  void runLoop();
  bool fetchTemplate(nlohmann::json& out);

  GbtAdapter* adapter_;
  config::PoolEntry pool_;
  std::shared_ptr<submit::GbtSubmitter> submitter_;
  std::atomic<bool> stop_flag_{false};
  std::thread th_;
};

}  // namespace adapters


