#pragma once

#include <algorithm>
#include <cstdint>

namespace normalize {

inline uint32_t clampRollingVersion(uint32_t version, uint32_t vmask) {
  // Only bits within vmask are allowed to change; others remain as-is.
  return (version & ~vmask) | (version & vmask);
}

inline uint32_t clampNtime(uint32_t ntime, uint32_t ntime_min, uint32_t ntime_max) {
  if (ntime < ntime_min) return ntime_min;
  if (ntime > ntime_max) return ntime_max;
  return ntime;
}

}  // namespace normalize


