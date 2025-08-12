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
  if (outbox_) {
    store::PendingSubmit ps{};
    ps.work_id = work_id;
    ps.nonce = nonce;
    std::memcpy(ps.header80, header80_be, 80);
    outbox_->enqueue(ps);
  }
  if (ledger_) {
    // If caller pre-populated ledger with this work_id, we keep as-is; otherwise ignore.
    (void)ledger_;
  }
  if (cb_) cb_(rec);
  return true;
}

bool SubmitRouter::routeRaw(const HitRecord& rec) {
  if (outbox_) {
    store::PendingSubmit ps{};
    ps.work_id = rec.work_id;
    ps.nonce = rec.nonce;
    std::memcpy(ps.header80, rec.header80, 80);
    outbox_->enqueue(ps);
  }
  if (cb_) { cb_(rec); return true; }
  return false;
}

}  // namespace submit


