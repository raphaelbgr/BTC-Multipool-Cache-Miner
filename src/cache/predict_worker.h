#pragma once

#include <cstdint>

#include "cache/cache_manager.h"

namespace cache {

struct GpuTelemetry {
  uint32_t totalVRAMMiB{0};
  uint32_t usedVRAMMiB{0};
  uint32_t gpuUtilPercent{0};
};

struct PredictDecision {
  bool throttle{false};           // suggest pausing allocations/launch size increase
  bool allowAllocate{true};       // whether new page allocations are allowed
  bool allowRelease{true};        // whether releasing pages is allowed
};

// PredictabilityWorker placeholder: pure function style decision engine.
class PredictabilityWorker {
 public:
  explicit PredictabilityWorker(Watermarks wm) : wm_(wm) {}

  void setWatermarks(Watermarks wm) { wm_ = wm; }
  Watermarks watermarks() const { return wm_; }

  PredictDecision evaluate(const GpuTelemetry& t) const {
    PredictDecision d{};
    const uint32_t freeMiB = (t.totalVRAMMiB > t.usedVRAMMiB) ? (t.totalVRAMMiB - t.usedVRAMMiB) : 0u;
    // Throttle if GPU util very high or free below minimum
    if (t.gpuUtilPercent >= 95 || freeMiB < wm_.minFreeMiB) d.throttle = true;

    // Allocation gate: require free over min and util below target + 5%
    d.allowAllocate = (freeMiB >= wm_.minFreeMiB) && (t.gpuUtilPercent <= (wm_.targetUtilPercent + 5));
    // Releases are allowed unless free is ample and util is low (avoid churn)
    d.allowRelease = !(freeMiB >= (wm_.minFreeMiB * 2) && t.gpuUtilPercent < (wm_.targetUtilPercent - 10));
    return d;
  }

 private:
  Watermarks wm_{};
};

}  // namespace cache


