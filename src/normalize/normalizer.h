#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>

#include "adapters/adapter_base.h"
#include "normalize/endianness.h"
#include "normalize/midstate.h"
#include "normalize/targets.h"
#include "registry/types.h"

namespace normalize {

struct NormalizerResult {
  registry::WorkItem item;
  registry::GpuJobConst job_const;
};

// Minimal normalizer from RawJob to WorkItem + GpuJobConst.
// For now, prevhash and merkle_root are provided as BE bytes and converted to LE words.
struct RawJobInputs {
  uint32_t source_id{0};
  adapters::RawJob job; // placeholder
  uint8_t prevhash_be[32]{};
  uint8_t merkle_root_be[32]{};
  uint8_t header_first64[64]{}; // for midstate (version..merkle_root)
  uint32_t share_nbits{0}; // varDiff share target compact
};

std::optional<NormalizerResult> normalizeJob(const RawJobInputs& in);

}




