#include "adapters/gbt_runner.h"

#include <chrono>
#include <iostream>
#include <sstream>

#include "net/rpc_client.h"
#include "normalize/normalizer.h"

namespace adapters {

GbtRunner::GbtRunner(GbtAdapter* adapter, const config::PoolEntry& pool_cfg)
    : adapter_(adapter), pool_(pool_cfg) {
  if (pool_.rpc.has_value()) {
    submitter_ = std::make_shared<submit::GbtSubmitter>(*pool_.rpc);
    adapter_->setSubmitter(submitter_);
  }
}

GbtRunner::~GbtRunner() { stop(); }

void GbtRunner::start() {
  if (th_.joinable()) return;
  stop_flag_.store(false);
  th_ = std::thread(&GbtRunner::runLoop, this);
}

void GbtRunner::stop() {
  stop_flag_.store(true);
  if (th_.joinable()) th_.join();
}

static void hexToBytes(const std::string& hex, uint8_t* out, size_t out_len) {
  size_t n = std::min(hex.size() / 2, out_len);
  for (size_t i = 0; i < n; i++) {
    unsigned int v; sscanf(hex.c_str() + 2 * i, "%02x", &v); out[i] = static_cast<uint8_t>(v);
  }
}

bool GbtRunner::fetchTemplate(nlohmann::json& out) {
  if (!pool_.rpc.has_value()) return false;
  net::RpcClient rpc(pool_.rpc->url);
  if (pool_.rpc->auth == "userpass") {
    rpc.setBasicAuth(pool_.rpc->username, pool_.rpc->password);
  } else {
    // cookie auth auto
  }
  nlohmann::json req = nlohmann::json::array();
  nlohmann::json tmpl;
  nlohmann::json param = nlohmann::json::object();
  // Rules per config (include segwit)
  nlohmann::json rules = nlohmann::json::array();
  if (pool_.gbt.has_value()) {
    for (const auto& r : pool_.gbt->rules) rules.push_back(r);
  } else {
    rules.push_back("segwit");
  }
  param["rules"] = rules;
  req.push_back(param);
  auto res = rpc.call("getblocktemplate", req);
  if (!res.has_value()) return false;
  out = *res;
  return true;
}

void GbtRunner::runLoop() {
  int poll_ms = 2000;
  if (pool_.gbt.has_value()) poll_ms = pool_.gbt->poll_ms;
  std::string last_prevhash;
  while (!stop_flag_.load()) {
    nlohmann::json gbt;
    if (!fetchTemplate(gbt)) {
      std::this_thread::sleep_for(std::chrono::milliseconds(poll_ms));
      continue;
    }
    try {
      std::string prevhash = gbt["previousblockhash"].get<std::string>();
      if (prevhash != last_prevhash) {
        last_prevhash = prevhash;
        normalize::RawJobInputs in;
        in.source_id = 0;
        in.job.source_name = "gbt";
        in.job.work_id = static_cast<uint64_t>(std::hash<std::string>{}(prevhash));
        // version in GBT can be int; get<int>(), then cast
        in.job.version = static_cast<uint32_t>(gbt["version"].get<int>());
        in.job.nbits = std::stoul(gbt["bits"].get<std::string>(), nullptr, 16);
        in.job.ntime = static_cast<uint32_t>(gbt["curtime"].get<int64_t>());
        // prevhash BE
        hexToBytes(prevhash, in.prevhash_be, 32);
        // For now, use GBT's provided merkle root if available; placeholder otherwise
        std::string mr;
        if (gbt.contains("default_witness_commitment") && gbt["default_witness_commitment"].is_string()) {
          mr = gbt["default_witness_commitment"].get<std::string>();
        } else if (gbt.contains("merkleroot") && gbt["merkleroot"].is_string()) {
          mr = gbt["merkleroot"].get<std::string>();
        } else {
          mr = std::string(64, '0');
        }
        hexToBytes(mr, in.merkle_root_be, 32);
        // share target: easy default
        in.share_nbits = in.job.nbits;
        in.clean_jobs = true;
        adapter_->ingestTemplate(in);
        if (submitter_) submitter_->updateTemplate(gbt);
      }
    } catch (...) {
      // ignore malformatted
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(poll_ms));
  }
}

} // namespace adapters


