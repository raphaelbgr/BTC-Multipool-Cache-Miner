#pragma once

#include <cstdint>

namespace cuda_engine {

struct AutoTuneInputs {
  uint32_t observedMsPerBatch{0}; // measured time for last batch
  uint32_t maxMsPerBatch{16};     // budget per batch (e.g., 16ms)
  uint32_t currentMicroBatch{1};  // current micro-batch size (blocks per job)
};

struct AutoTuneDecision {
  uint32_t nextMicroBatch{1};
  bool increased{false};
  bool decreased{false};
};

inline AutoTuneDecision autoTuneMicroBatch(const AutoTuneInputs& in) {
  AutoTuneDecision d{in.currentMicroBatch, false, false};
  if (in.observedMsPerBatch == 0) return d;
  // If under budget by >20%, increase micro-batch by 2x (up to a limit)
  if (in.observedMsPerBatch * 5 < in.maxMsPerBatch * 4) {
    uint64_t next = static_cast<uint64_t>(in.currentMicroBatch) * 2ull;
    if (next > 1'000'000ull) next = 1'000'000ull;
    d.nextMicroBatch = static_cast<uint32_t>(next);
    d.increased = d.nextMicroBatch != in.currentMicroBatch;
    return d;
  }
  // If over budget, decrease by half (down to 1)
  if (in.observedMsPerBatch > in.maxMsPerBatch) {
    uint32_t next = in.currentMicroBatch > 1 ? (in.currentMicroBatch / 2) : 1u;
    d.nextMicroBatch = next;
    d.decreased = d.nextMicroBatch != in.currentMicroBatch;
    return d;
  }
  return d;
}

}  // namespace cuda_engine


