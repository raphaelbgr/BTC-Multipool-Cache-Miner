#pragma once

#include <mutex>
#include <string_view>
#include <optional>
#include <queue>
#include <string>

#include "adapters/adapter_base.h"
#include "normalize/normalizer.h"
#include "normalize/targets.h"

namespace adapters {

class StratumAdapter : public AdapterBase {
 public:
  explicit StratumAdapter(std::string name) : name_(std::move(name)) {}

  std::optional<registry::WorkItem> pollNormalized() override;
  void submitShare(uint64_t work_id, uint32_t nonce, const uint8_t header80_be[80]) override;

  // For tests: enqueue a dummy job
  void enqueueDummy(uint64_t work_id, uint32_t nbits);

  // Ingest a Stratum job (already parsed) and normalize it.
  // Minimal fields for now; future: coinbase parts and witness commitment.
  void ingestJob(const normalize::RawJobInputs& inputs);

  // Returns both WorkItem and GpuJobConst when available
  std::optional<normalize::NormalizerResult> pollNormalizedFull();

  // Minimal internal state machine (no networking yet)
  enum class State { kDisconnected, kConnecting, kSubscribed, kAuthorized };

  void connect();
  void onSubscribed(std::string_view extranonce1_hex, uint8_t extranonce2_size);
  void onAuthorizeOk();

  State state() const;
  uint8_t extranonce2Size() const;

 private:
  std::string name_;
  mutable std::mutex mu_;
  std::queue<normalize::NormalizerResult> q_;

  // State guarded by mu_
  State state_{State::kDisconnected};
  uint8_t extranonce2_size_{0};
  std::string extranonce1_hex_{};  // stored for completeness
};

}  // namespace adapters




