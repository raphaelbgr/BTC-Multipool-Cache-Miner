#include "adapters/gbt_adapter.h"

namespace adapters {

std::optional<registry::WorkItem> GbtAdapter::pollNormalized() {
  std::lock_guard<std::mutex> lock(mu_);
  if (q_.empty()) return std::nullopt;
  auto it = q_.front();
  q_.pop();
  return it.item;
}

void GbtAdapter::submitShare(uint64_t, uint32_t, const uint8_t[80]) {
  // Placeholder: would submit via RPC in a real implementation.
}

void GbtAdapter::ingestTemplate(const normalize::RawJobInputs& inputs) {
  auto out = normalize::normalizeJob(inputs);
  if (!out.has_value()) return;
  std::lock_guard<std::mutex> lock(mu_);
  q_.push(*out);
}

std::optional<normalize::NormalizerResult> GbtAdapter::pollNormalizedFull() {
  std::lock_guard<std::mutex> lock(mu_);
  if (q_.empty()) return std::nullopt;
  auto it = q_.front();
  q_.pop();
  return it;
}

}  // namespace adapters


