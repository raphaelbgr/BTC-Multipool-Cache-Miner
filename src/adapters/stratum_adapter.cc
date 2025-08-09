#include "adapters/stratum_adapter.h"

#include <utility>

namespace adapters {

std::optional<registry::WorkItem> StratumAdapter::pollNormalized() {
  std::lock_guard<std::mutex> lock(mu_);
  if (q_.empty()) return std::nullopt;
  auto it = q_.front();
  q_.pop();
  return it.item;
}

void StratumAdapter::submitShare(uint64_t, uint32_t, const uint8_t[80]) {
  // Placeholder: real implementation will POST via Stratum submit.
}

void StratumAdapter::enqueueDummy(uint64_t work_id, uint32_t nbits) {
  normalize::NormalizerResult r{};
  registry::WorkItem& w = r.item;
  w.work_id = work_id;
  w.source_id = 0;
  w.version = 0x20000000u;
  w.ntime = 0;
  w.nbits = nbits;
  w.nonce_start = 0;
  w.block_target_le = normalize::compactToTargetU32LE(nbits);
  w.share_target_le = w.block_target_le;
  std::lock_guard<std::mutex> lock(mu_);
  q_.push(r);
}

void StratumAdapter::ingestJob(const normalize::RawJobInputs& inputs) {
  auto out = normalize::normalizeJob(inputs);
  if (!out.has_value()) return;
  std::lock_guard<std::mutex> lock(mu_);
  q_.push(*out);
}

void StratumAdapter::connect() {
  std::lock_guard<std::mutex> lock(mu_);
  state_ = State::kConnecting;
}

void StratumAdapter::onSubscribed(std::string_view extranonce1_hex, uint8_t extranonce2_size) {
  std::lock_guard<std::mutex> lock(mu_);
  extranonce1_hex_ = std::string(extranonce1_hex);
  extranonce2_size_ = extranonce2_size;
  state_ = State::kSubscribed;
}

void StratumAdapter::onAuthorizeOk() {
  std::lock_guard<std::mutex> lock(mu_);
  state_ = State::kAuthorized;
}

StratumAdapter::State StratumAdapter::state() const {
  std::lock_guard<std::mutex> lock(mu_);
  return state_;
}

uint8_t StratumAdapter::extranonce2Size() const {
  std::lock_guard<std::mutex> lock(mu_);
  return extranonce2_size_;
}

std::optional<normalize::NormalizerResult> StratumAdapter::pollNormalizedFull() {
  std::lock_guard<std::mutex> lock(mu_);
  if (q_.empty()) return std::nullopt;
  auto it = q_.front();
  q_.pop();
  return it;
}

}  // namespace adapters




