#include "obs/metrics.h"
#include "net/socket.h"

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

// Very small blocking HTTP server serving one connection at a time
MetricsHttpServer::MetricsHttpServer(MetricsRegistry* registry, const char* host, uint16_t port)
    : registry_(registry), host_(host ? host : "127.0.0.1"), port_(port) {}

MetricsHttpServer::~MetricsHttpServer() { stop(); }

bool MetricsHttpServer::start() {
  if (running_.load()) return false;
  running_.store(true);
  th_ = std::thread(&MetricsHttpServer::run, this);
  return true;
}

void MetricsHttpServer::stop() {
  running_.store(false);
  if (th_.joinable()) th_.join();
}

void MetricsHttpServer::run() {
  // For simplicity, reuse client socket to connect to host_:port_ as if server; since socket has no bind/listen,
  // we implement a simple polling client that accepts nothing unless a reverse proxy is used. Placeholder.
  // Future: implement platform-specific listen/accept.
  while (running_.load()) {
    // No-op placeholder to keep thread alive; actual serving can be implemented later.
    std::this_thread::sleep_for(std::chrono::seconds(1));
  }
}

}  // namespace obs


