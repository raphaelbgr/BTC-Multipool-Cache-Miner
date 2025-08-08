#pragma once

#include <cstddef>
#include <mutex>
#include <optional>
#include <vector>

#include "registry/types.h"

namespace registry {

class WorkSourceRegistry {
 public:
  explicit WorkSourceRegistry(std::size_t num_slots);

  std::size_t size() const noexcept { return slots_.size(); }

  // Replace the slot (item + gpu const) in-place and bump generation counter last.
  void set(std::size_t slot_index, const WorkItem& item, const GpuJobConst& job_const);

  // Snapshot a copy of the slot (gen + item). Returns nullopt if out of range.
  std::optional<WorkSlotSnapshot> get(std::size_t slot_index) const;

 private:
  std::vector<WorkSlot> slots_;
};

}  // namespace registry


