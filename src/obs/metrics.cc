#include "obs/metrics.h"

namespace obs {

void MetricsRegistry::increment(const std::string& name, uint64_t delta) {
  std::lock_guard<std::mutex> lock(mu_);
  counters_[name] += delta;
}

void MetricsRegistry::setGauge(const std::string& name, int64_t value) {
  std::lock_guard<std::mutex> lock(mu_);
  gauges_[name] = value;
}

nlohmann::json MetricsRegistry::snapshot() const {
  std::lock_guard<std::mutex> lock(mu_);
  nlohmann::json j;
  for (const auto& [k, v] : counters_) j["counters"][k] = v;
  for (const auto& [k, v] : gauges_) j["gauges"][k] = v;
  return j;
}

}  // namespace obs


