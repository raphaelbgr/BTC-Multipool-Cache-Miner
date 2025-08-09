#include "cache/vram_pages.h"

namespace cache {

VramPageId VramPages::create(uint32_t sizeMiB) {
  std::lock_guard<std::mutex> lock(mu_);
  const VramPageId id = next_id_++;
  pages_[id] = VramPageMeta{ id, sizeMiB, false };
  return id;
}

bool VramPages::markInUse(VramPageId id, bool in_use) {
  std::lock_guard<std::mutex> lock(mu_);
  auto it = pages_.find(id);
  if (it == pages_.end()) return false;
  it->second.inUse = in_use;
  return true;
}

bool VramPages::destroy(VramPageId id) {
  std::lock_guard<std::mutex> lock(mu_);
  return pages_.erase(id) > 0;
}

uint32_t VramPages::totalPages() const {
  std::lock_guard<std::mutex> lock(mu_);
  return static_cast<uint32_t>(pages_.size());
}

}  // namespace cache


