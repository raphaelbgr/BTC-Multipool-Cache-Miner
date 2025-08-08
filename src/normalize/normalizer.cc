// Copyright
#include "normalize/normalizer.h"

namespace normalize {

std::optional<NormalizerResult> normalizeJob(const RawJobInputs& in) {
  NormalizerResult r;

  // Fill WorkItem identity and header fields
  r.item.work_id = in.job.work_id;
  r.item.source_id = in.source_id;
  r.item.version = in.job.version;
  r.item.nbits = in.job.nbits;
  r.item.ntime = in.job.ntime;
  r.item.nonce_start = 0;

  // Convert prevhash and merkle root to LE u32 words for kernel
  r.item.prevhash_le = normalize::be32BytesToLeU32Words(in.prevhash_be);
  r.item.merkle_root_le = normalize::be32BytesToLeU32Words(in.merkle_root_be);

  // Targets
  r.item.block_target_le = normalize::compactToTargetU32LE(in.job.nbits);
  r.item.share_target_le = normalize::compactToTargetU32LE(in.share_nbits);

  // Policy defaults
  r.item.vmask = 0;
  r.item.ntime_min = r.item.ntime;
  r.item.ntime_max = r.item.ntime;
  r.item.extranonce2_size = 0;
  r.item.clean_jobs = true;
  r.item.active = true;
  r.item.found_submitted = false;

  // Midstate for first 64 bytes of header
  r.job_const.midstate_le = normalize::sha256_midstate_64(in.header_first64);

  return r;
}

}  // namespace normalize

