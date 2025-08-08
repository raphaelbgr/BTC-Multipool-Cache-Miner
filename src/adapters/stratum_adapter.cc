#include "adapters/stratum_adapter.h"

#include <utility>

namespace adapters {

std::optional<registry::WorkItem> StratumAdapter::pollNormalized() {
  std::lock_guard<std::mutex> lock(mu_);
  if (q_.empty()) return std::nullopt;
  auto it = q_.front();
  q_.pop();
  return it;
}

void StratumAdapter::submitShare(uint64_t, uint32_t, const uint8_t[80]) {
  // Placeholder: real implementation will POST via Stratum submit.
}

void StratumAdapter::enqueueDummy(uint64_t work_id, uint32_t nbits) {
  registry::WorkItem w{};
  w.work_id = work_id;
  w.source_id = 0;
  w.version = 0x20000000u;
  w.ntime = 0;
  w.nbits = nbits;
  w.nonce_start = 0;
  w.block_target_le = normalize::compactToTargetU32LE(nbits);
  w.share_target_le = w.block_target_le;
  std::lock_guard<std::mutex> lock(mu_);
  q_.push(w);
}

}  // namespace adapters




