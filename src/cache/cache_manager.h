#pragma once

#include <cstdint>
#include <mutex>

#include "cache/vram_pages.h"

namespace cache {

struct Watermarks {
  uint32_t targetUtilPercent{85}; // desired VRAM utilization
  uint32_t minFreeMiB{256};
};

class CacheManager {
 public:
  explicit CacheManager(Watermarks wm) : wm_(wm) {}

  VramPageId allocate(uint32_t sizeMiB);
  bool release(VramPageId id);
  void setWatermarks(Watermarks wm);
  Watermarks watermarks() const;

 private:
  mutable std::mutex mu_;
  Watermarks wm_{};
  VramPages pages_;
};

}  // namespace cache


