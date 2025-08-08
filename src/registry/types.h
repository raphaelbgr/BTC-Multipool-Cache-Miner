#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <string>

namespace registry {

struct GpuJobConst {
  // Midstate of first 64 bytes of block header (first round SHA-256)
  std::array<uint32_t, 8> midstate_le{};
};

struct WorkItem {
  // Identity and source association
  uint64_t work_id{0};            // Unique per job across all sources
  uint32_t source_id{0};          // Slot index / source identifier

  // Core header fields
  uint32_t version{0};
  uint32_t ntime{0};
  uint32_t nbits{0};
  uint32_t nonce_start{0};        // Starting nonce for kernel batch

  // Normalized little-endian words for kernel-ready data
  std::array<uint32_t, 8> prevhash_le{};      // 32-byte prevhash as LE u32 words
  std::array<uint32_t, 8> merkle_root_le{};   // 32-byte merkle root as LE u32 words
  std::array<uint32_t, 8> share_target_le{};  // varDiff share target
  std::array<uint32_t, 8> block_target_le{};  // nBits-derived block target

  // Policy and flags
  uint32_t vmask{0};
  uint32_t ntime_min{0};
  uint32_t ntime_max{0};
  uint8_t extranonce2_size{0};
  bool clean_jobs{false};
  bool active{false};
  bool found_submitted{false};
};

struct WorkSlot {
  std::atomic<uint64_t> gen{0};
  WorkItem item{};
  GpuJobConst job_const{};
};

struct WorkSlotSnapshot {
  uint64_t gen{0};
  WorkItem item{};
  GpuJobConst job_const{};
};

}  // namespace registry


