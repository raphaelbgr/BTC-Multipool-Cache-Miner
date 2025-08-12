#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>

#include "registry/work_source_registry.h"

namespace scheduler {

struct SchedulerState {
  std::unordered_map<uint32_t, uint32_t> source_weight; // default 1
  uint64_t tick{0};
  // Optional backpressure: per-source penalty (0..N) reduces effective weight
  std::unordered_map<uint32_t, uint32_t> source_penalty;

  std::vector<uint64_t> select(const std::vector<uint64_t>& work_ids,
                               const std::unordered_map<uint64_t, registry::WorkSlotSnapshot>& snaps,
                               std::size_t max_ids = 64) {
    std::vector<uint64_t> out;
    out.reserve(work_ids.size());
    for (auto wid : work_ids) {
      auto it = snaps.find(wid);
      if (it == snaps.end()) continue;
      uint32_t sid = it->second.item.source_id;
      uint32_t w = 1u;
      auto wit = source_weight.find(sid);
      if (wit != source_weight.end() && wit->second > 0) w = (wit->second > 4u ? 4u : wit->second);
      auto pit = source_penalty.find(sid);
      if (pit != source_penalty.end() && pit->second > 0) {
        if (pit->second >= w) w = 1u; else w = w - pit->second;
      }
      for (uint32_t k = 0; k < w; ++k) {
        out.push_back(wid);
        if (out.size() >= max_ids) break;
      }
      if (out.size() >= max_ids) break;
    }
    tick++;
    return out;
  }
};

}  // namespace scheduler


