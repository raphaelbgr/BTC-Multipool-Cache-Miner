#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "submit/cpu_verify.h"

namespace submit {

struct HitRecord {
  uint64_t work_id{0};
  uint32_t nonce{0};
  uint8_t header80[80]{}; // big-endian fields as transmitted
};

class SubmitRouter {
 public:
  using SubmitCallback = std::function<void(const HitRecord&)>;

  explicit SubmitRouter(SubmitCallback cb) : cb_(std::move(cb)) {}

  // Verify header double SHA256 <= target_le; if valid, route via callback.
  bool verifyAndSubmit(const uint8_t header80_be[80], const std::array<uint32_t, 8>& target_le,
                       uint64_t work_id, uint32_t nonce);

 private:
  SubmitCallback cb_;
};

}  // namespace submit


