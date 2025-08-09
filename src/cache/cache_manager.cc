#include "cache/cache_manager.h"

namespace cache {

VramPageId CacheManager::allocate(uint32_t sizeMiB) {
  std::lock_guard<std::mutex> lock(mu_);
  // Placeholder policy: just create
  return pages_.create(sizeMiB);
}

bool CacheManager::release(VramPageId id) {
  std::lock_guard<std::mutex> lock(mu_);
  return pages_.destroy(id);
}

void CacheManager::setWatermarks(Watermarks wm) {
  std::lock_guard<std::mutex> lock(mu_);
  wm_ = wm;
}

Watermarks CacheManager::watermarks() const {
  std::lock_guard<std::mutex> lock(mu_);
  return wm_;
}

}  // namespace cache


