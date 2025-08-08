#include "registry/work_source_registry.h"

#include <stdexcept>

namespace registry {

WorkSourceRegistry::WorkSourceRegistry(std::size_t num_slots) : slots_(num_slots) {
  if (num_slots == 0) throw std::invalid_argument("num_slots must be > 0");
}

void WorkSourceRegistry::set(std::size_t slot_index, const WorkItem& item) {
  if (slot_index >= slots_.size()) throw std::out_of_range("slot_index");
  WorkSlot& slot = slots_[slot_index];
  // In-place replace, then bump gen last to signal availability
  slot.item = item;
  slot.gen.fetch_add(1, std::memory_order_release);
}

std::optional<WorkSlotSnapshot> WorkSourceRegistry::get(std::size_t slot_index) const {
  if (slot_index >= slots_.size()) return std::nullopt;
  const WorkSlot& src = slots_[slot_index];
  WorkSlotSnapshot snap;
  // Acquire to see latest item after gen published
  snap.gen = src.gen.load(std::memory_order_acquire);
  snap.item = src.item;
  return snap;
}

}  // namespace registry


