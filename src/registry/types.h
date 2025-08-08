#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <string>

namespace registry {

struct GpuJobConst {
  std::array<uint32_t, 8> target_le{};  // LE u32 words
  std::array<uint32_t, 8> midstate_le{};
};

struct WorkItem {
  uint64_t work_id{0};
  uint32_t version{0};
  uint32_t ntime{0};
  uint32_t nonce_start{0};
  GpuJobConst job_const{};
};

struct WorkSlot {
  std::atomic<uint64_t> gen{0};
  WorkItem item{};
};

struct WorkSlotSnapshot {
  uint64_t gen{0};
  WorkItem item{};
};

}  // namespace registry


