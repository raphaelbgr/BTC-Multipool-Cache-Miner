#pragma once

#include <mutex>
#include <optional>
#include <queue>
#include <string>

#include "adapters/adapter_base.h"
#include "normalize/targets.h"

namespace adapters {

class StratumAdapter : public AdapterBase {
 public:
  explicit StratumAdapter(std::string name) : name_(std::move(name)) {}

  std::optional<registry::WorkItem> pollNormalized() override;
  void submitShare(uint64_t work_id, uint32_t nonce, const uint8_t header80_be[80]) override;

  // For tests: enqueue a dummy job
  void enqueueDummy(uint64_t work_id, uint32_t nbits);

 private:
  std::string name_;
  std::mutex mu_;
  std::queue<registry::WorkItem> q_;
};

}  // namespace adapters




