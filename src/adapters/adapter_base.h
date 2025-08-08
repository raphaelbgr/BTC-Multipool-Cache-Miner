#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

#include "registry/types.h"

namespace adapters {

// Minimal RawJob placeholder; real implementations will flesh this out
struct RawJob {
  std::string source_name;
  uint64_t work_id{0};
  uint32_t version{0};
  uint32_t nbits{0};
  uint32_t ntime{0};
};

class AdapterBase {
 public:
  virtual ~AdapterBase() = default;

  // Non-blocking poll; returns a new normalized WorkItem when available
  virtual std::optional<registry::WorkItem> pollNormalized() = 0;

  // Submit a found share/block identified by work_id
  virtual void submitShare(uint64_t work_id, uint32_t nonce, const uint8_t header80_be[80]) = 0;
};

using AdapterPtr = std::shared_ptr<AdapterBase>;

}  // namespace adapters




