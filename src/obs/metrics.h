#pragma once

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>
#include <nlohmann/json.hpp>

namespace obs {

class MetricsRegistry {
 public:
  void increment(const std::string& name, uint64_t delta = 1);
  void setGauge(const std::string& name, int64_t value);

  nlohmann::json snapshot() const;

 private:
  // Simple thread-safe maps guarded by a mutex (low cardinality expected initially)
  mutable std::mutex mu_;
  std::unordered_map<std::string, uint64_t> counters_;
  std::unordered_map<std::string, int64_t> gauges_;
};

}  // namespace obs


