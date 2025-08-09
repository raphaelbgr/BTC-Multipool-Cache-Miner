#pragma once

#include <cstdint>
#include <mutex>
#include <optional>
#include <unordered_map>

namespace cache {

using VramPageId = uint64_t;

struct VramPageMeta {
  VramPageId id{0};
  uint32_t sizeMiB{0};
  bool inUse{false};
};

class VramPages {
 public:
  VramPages() = default;

  VramPageId create(uint32_t sizeMiB);
  bool markInUse(VramPageId id, bool in_use);
  bool destroy(VramPageId id);
  uint32_t totalPages() const;

 private:
  mutable std::mutex mu_;
  VramPageId next_id_{1};
  std::unordered_map<VramPageId, VramPageMeta> pages_;
};

}  // namespace cache


