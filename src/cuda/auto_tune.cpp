#include "cuda/auto_tune.h"

#include <fstream>
#include <nlohmann/json.hpp>

namespace cuda_engine {

static std::string pathFor(const std::string& device_name) {
  return std::string("logs/") + device_name + std::string(".tuning.json");
}

bool loadTuningProfile(const std::string& device_name, TuningProfile* out) {
  if (!out) return false;
  std::ifstream ifs(pathFor(device_name));
  if (!ifs.good()) return false;
  nlohmann::json j; try { ifs >> j; } catch (...) { return false; }
  try {
    out->threads_per_block = j.value("threads_per_block", out->threads_per_block);
    out->nonces_per_thread = j.value("nonces_per_thread", out->nonces_per_thread);
  } catch (...) { return false; }
  return true;
}

bool saveTuningProfile(const std::string& device_name, const TuningProfile& prof) {
  nlohmann::json j;
  j["threads_per_block"] = prof.threads_per_block;
  j["nonces_per_thread"] = prof.nonces_per_thread;
  std::ofstream ofs(pathFor(device_name), std::ios::out | std::ios::trunc);
  if (!ofs.good()) return false;
  ofs << j.dump();
  return true;
}

}


