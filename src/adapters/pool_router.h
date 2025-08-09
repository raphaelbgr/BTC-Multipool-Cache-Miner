#pragma once

#include <cstddef>
#include <optional>
#include <vector>

#include "adapters/adapter_base.h"

namespace adapters {

class PoolRouter {
 public:
  enum class Strategy { kFailover, kRoundRobin };

  explicit PoolRouter(Strategy strategy) : strategy_(strategy) {}

  void addAdapter(const AdapterPtr& adapter) { adapters_.push_back(adapter); }
  void setStrategy(Strategy s) { strategy_ = s; }
  std::size_t size() const { return adapters_.size(); }

  std::optional<registry::WorkItem> pollNext();

 private:
  Strategy strategy_{Strategy::kFailover};
  std::vector<AdapterPtr> adapters_;
  std::size_t rr_index_{0};
};

}  // namespace adapters


