#include "normalize/merkle.h"

#include <array>
#include <cstring>

#include "submit/cpu_verify.h"

namespace normalize {

void compute_merkle_root_be(const std::vector<std::array<uint8_t, 32>>& txids_be,
                            uint8_t out32_be[32]) {
  if (txids_be.empty()) {
    std::memset(out32_be, 0, 32);
    return;
  }
  std::vector<std::array<uint8_t, 32>> level = txids_be;
  while (level.size() > 1) {
    std::vector<std::array<uint8_t, 32>> next;
    next.reserve((level.size() + 1) / 2);
    for (size_t i = 0; i < level.size(); i += 2) {
      const auto& a = level[i];
      const auto& b = (i + 1 < level.size()) ? level[i + 1] : level[i];
      uint8_t buf[64];
      std::memcpy(buf, a.data(), 32);
      std::memcpy(buf + 32, b.data(), 32);
      std::array<uint8_t, 32> h{};
      submit::sha256d(buf, 64, h.data());
      next.emplace_back(h);
    }
    level.swap(next);
  }
  std::memcpy(out32_be, level[0].data(), 32);
}

}  // namespace normalize


