#pragma once

#include <cstdint>
#include <fstream>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include <nlohmann/json.hpp>

#include "registry/types.h"

namespace store {

// Minimal in-memory ledger for mapping work_id -> WorkItem snapshot.
// Intended to be replaced by mmap-backed persistence later.
class Ledger {
 public:
  Ledger() = default;

  void put(const registry::WorkItem& item) {
    std::lock_guard<std::mutex> lock(mu_);
    work_map_[item.work_id] = item;
  }

  std::optional<registry::WorkItem> get(uint64_t work_id) const {
    std::lock_guard<std::mutex> lock(mu_);
    auto it = work_map_.find(work_id);
    if (it == work_map_.end()) return std::nullopt;
    return it->second;
  }

  void erase(uint64_t work_id) {
    std::lock_guard<std::mutex> lock(mu_);
    work_map_.erase(work_id);
  }

    // Persistence (JSONL: one JSON object per line)
    bool saveToFile(const std::string& filepath) const {
      std::lock_guard<std::mutex> lock(mu_);
      std::ofstream ofs(filepath, std::ios::binary | std::ios::trunc);
      if (!ofs) return false;
      for (const auto& [work_id, item] : work_map_) {
        nlohmann::json j;
        j["work_id"] = item.work_id;
        j["source_id"] = item.source_id;
        j["version"] = item.version;
        j["ntime"] = item.ntime;
        j["nbits"] = item.nbits;
        j["nonce_start"] = item.nonce_start;
        auto arr_to_vec = [](const std::array<uint32_t,8>& a){ std::vector<uint32_t> v(a.begin(), a.end()); return v; };
        j["prevhash_le"] = arr_to_vec(item.prevhash_le);
        j["merkle_root_le"] = arr_to_vec(item.merkle_root_le);
        j["share_target_le"] = arr_to_vec(item.share_target_le);
        j["block_target_le"] = arr_to_vec(item.block_target_le);
        j["vmask"] = item.vmask;
        j["ntime_min"] = item.ntime_min;
        j["ntime_max"] = item.ntime_max;
        j["extranonce2_size"] = item.extranonce2_size;
        j["clean_jobs"] = item.clean_jobs;
        j["active"] = item.active;
        j["found_submitted"] = item.found_submitted;
        ofs << j.dump() << '\n';
      }
      return true;
    }

    bool loadFromFile(const std::string& filepath) {
      std::lock_guard<std::mutex> lock(mu_);
      std::ifstream ifs(filepath, std::ios::binary);
      if (!ifs) return false;
      work_map_.clear();
      std::string line;
      while (std::getline(ifs, line)) {
        if (line.empty()) continue;
        nlohmann::json j = nlohmann::json::parse(line, nullptr, false);
        if (j.is_discarded()) continue;
        registry::WorkItem item{};
        item.work_id = j.value("work_id", 0ull);
        item.source_id = j.value("source_id", 0u);
        item.version = j.value("version", 0u);
        item.ntime = j.value("ntime", 0u);
        item.nbits = j.value("nbits", 0u);
        item.nonce_start = j.value("nonce_start", 0u);
        auto vec_to_arr = [](const std::vector<uint32_t>& v){
          std::array<uint32_t,8> a{}; for (size_t i=0;i<a.size() && i<v.size();++i) a[i]=v[i]; return a; };
        item.prevhash_le = vec_to_arr(j.value("prevhash_le", std::vector<uint32_t>{}));
        item.merkle_root_le = vec_to_arr(j.value("merkle_root_le", std::vector<uint32_t>{}));
        item.share_target_le = vec_to_arr(j.value("share_target_le", std::vector<uint32_t>{}));
        item.block_target_le = vec_to_arr(j.value("block_target_le", std::vector<uint32_t>{}));
        item.vmask = j.value("vmask", 0u);
        item.ntime_min = j.value("ntime_min", 0u);
        item.ntime_max = j.value("ntime_max", 0u);
        item.extranonce2_size = static_cast<uint8_t>(j.value("extranonce2_size", 0u));
        item.clean_jobs = j.value("clean_jobs", false);
        item.active = j.value("active", false);
        item.found_submitted = j.value("found_submitted", false);
        work_map_[item.work_id] = item;
      }
      return true;
    }

 private:
  mutable std::mutex mu_;
  std::unordered_map<uint64_t, registry::WorkItem> work_map_;
};

}  // namespace store





