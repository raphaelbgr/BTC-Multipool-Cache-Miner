#include "submit/submit_router.h"

#include <cstring>

namespace submit {

bool SubmitRouter::verifyAndSubmit(const uint8_t header80_be[80], const std::array<uint32_t, 8>& target_le,
                                   uint64_t work_id, uint32_t nonce) {
  uint8_t h1[32];
  sha256(header80_be, 80, h1);
  uint8_t h2[32];
  sha256(h1, 32, h2);
  if (!isHashLEQTarget(h2, target_le)) return false;
  HitRecord rec;
  rec.work_id = work_id;
  rec.nonce = nonce;
  std::memcpy(rec.header80, header80_be, 80);
  if (cb_) cb_(rec);
  return true;
}

}  // namespace submit


