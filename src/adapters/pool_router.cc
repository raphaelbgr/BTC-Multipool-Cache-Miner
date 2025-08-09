#include "adapters/pool_router.h"

namespace adapters {

std::optional<registry::WorkItem> PoolRouter::pollNext() {
  if (adapters_.empty()) return std::nullopt;
  if (strategy_ == Strategy::kFailover) {
    for (const auto& a : adapters_) {
      if (!a) continue;
      auto w = a->pollNormalized();
      if (w.has_value()) return w;
    }
    return std::nullopt;
  }
  // Round robin: rotate start index each call and try all
  const std::size_t n = adapters_.size();
  for (std::size_t i = 0; i < n; ++i) {
    std::size_t idx = (rr_index_ + i) % n;
    const auto& a = adapters_[idx];
    if (!a) continue;
    auto w = a->pollNormalized();
    if (w.has_value()) {
      rr_index_ = (idx + 1) % n;  // next call starts after the adapter that produced work
      return w;
    }
  }
  rr_index_ = (rr_index_ + 1) % n;
  return std::nullopt;
}

}  // namespace adapters


