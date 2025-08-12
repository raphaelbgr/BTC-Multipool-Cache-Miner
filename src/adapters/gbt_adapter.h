#pragma once

#include <mutex>
#include <optional>
#include <queue>
#include <string>

#include "adapters/adapter_base.h"
#include "normalize/normalizer.h"
#include "normalize/targets.h"
#include "submit/gbt_submitter.h"

namespace adapters {

// Minimal GBT adapter skeleton: HTTP/JSON-RPC not implemented; acts as an ingest/normalize queue.
class GbtAdapter : public AdapterBase {
 public:
  explicit GbtAdapter(std::string name) : name_(std::move(name)) {}

  std::optional<registry::WorkItem> pollNormalized() override;
  void submitShare(uint64_t work_id, uint32_t nonce, const uint8_t header80_be[80]) override;

  // Ingest a pre-parsed block template into the normalizer.
  void ingestTemplate(const normalize::RawJobInputs& inputs);

  // Returns both WorkItem and GpuJobConst when available
  std::optional<normalize::NormalizerResult> pollNormalizedFull();

  // Wire a submitter (optional). If present, submitShare will call submitblock.
  void setSubmitter(std::shared_ptr<submit::GbtSubmitter> submitter) { submitter_ = std::move(submitter); }

 private:
  std::string name_;
  std::mutex mu_;
  std::queue<normalize::NormalizerResult> q_;
  std::shared_ptr<submit::GbtSubmitter> submitter_;
};

}  // namespace adapters


