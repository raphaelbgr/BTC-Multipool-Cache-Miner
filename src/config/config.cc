#include "config/config.h"

#include <fstream>
#include <nlohmann/json.hpp>

namespace config {

static CredMode parseCredMode(const std::string& s) {
  if (s == "account_worker") return CredMode::AccountWorker;
  return CredMode::WalletAsUser;
}

AppConfig loadFromJsonFile(const std::string& path) {
  AppConfig cfg;
  std::ifstream ifs(path);
  if (!ifs.good()) return cfg;
  nlohmann::json j; try { ifs >> j; } catch (...) { return cfg; }
  if (j.contains("log_level")) cfg.log_level = j["log_level"].get<int>();
  if (j.contains("scheduler") && j["scheduler"].is_object()) {
    const auto& s = j["scheduler"];
    if (s.contains("latency_penalty_ms")) cfg.scheduler.latency_penalty_ms = s["latency_penalty_ms"].get<int>();
    if (s.contains("max_weight")) cfg.scheduler.max_weight = s["max_weight"].get<int>();
  }
  if (j.contains("metrics") && j["metrics"].is_object()) {
    const auto& m = j["metrics"];
    if (m.contains("enable_file")) cfg.metrics.enable_file = m["enable_file"].get<bool>();
    if (m.contains("file_path")) cfg.metrics.file_path = m["file_path"].get<std::string>();
    if (m.contains("dump_interval_ms")) cfg.metrics.dump_interval_ms = m["dump_interval_ms"].get<int>();
    if (m.contains("enable_http")) cfg.metrics.enable_http = m["enable_http"].get<bool>();
    if (m.contains("http_host")) cfg.metrics.http_host = m["http_host"].get<std::string>();
    if (m.contains("http_port")) cfg.metrics.http_port = static_cast<uint16_t>(m["http_port"].get<int>());
    if (m.contains("file_max_bytes")) cfg.metrics.file_max_bytes = m["file_max_bytes"].get<uint64_t>();
    if (m.contains("file_rotate_interval_sec")) cfg.metrics.file_rotate_interval_sec = m["file_rotate_interval_sec"].get<uint64_t>();
  }
  if (j.contains("cuda") && j["cuda"].is_object()) {
    const auto& c = j["cuda"];
    if (c.contains("hit_ring_capacity")) cfg.cuda.hit_ring_capacity = c["hit_ring_capacity"].get<int>();
    if (c.contains("desired_threads_per_job")) cfg.cuda.desired_threads_per_job = c["desired_threads_per_job"].get<int>();
    if (c.contains("nonces_per_thread")) cfg.cuda.nonces_per_thread = c["nonces_per_thread"].get<int>();
    if (c.contains("budget_ms")) cfg.cuda.budget_ms = c["budget_ms"].get<int>();
  }
  if (j.contains("tls") && j["tls"].is_object()) {
    const auto& t = j["tls"];
    if (t.contains("verify")) cfg.tls.verify = t["verify"].get<bool>();
    if (t.contains("ca_file")) cfg.tls.ca_file = t["ca_file"].get<std::string>();
    if (t.contains("use_schannel")) cfg.tls.use_schannel = t["use_schannel"].get<bool>();
  }
  if (j.contains("outbox") && j["outbox"].is_object()) {
    const auto& o = j["outbox"];
    if (o.contains("path")) cfg.outbox.path = o["path"].get<std::string>();
    if (o.contains("max_bytes")) cfg.outbox.max_bytes = o["max_bytes"].get<uint64_t>();
    if (o.contains("rotate_on_start")) cfg.outbox.rotate_on_start = o["rotate_on_start"].get<bool>();
    if (o.contains("rotate_interval_sec")) cfg.outbox.rotate_interval_sec = o["rotate_interval_sec"].get<uint64_t>();
  }
  if (j.contains("ledger") && j["ledger"].is_object()) {
    const auto& l = j["ledger"];
    if (l.contains("path")) cfg.ledger.path = l["path"].get<std::string>();
    if (l.contains("max_bytes")) cfg.ledger.max_bytes = l["max_bytes"].get<uint64_t>();
    if (l.contains("rotate_interval_sec")) cfg.ledger.rotate_interval_sec = l["rotate_interval_sec"].get<uint64_t>();
  }
  if (j.contains("pools") && j["pools"].is_array()) {
    for (const auto& p : j["pools"]) {
      PoolEntry e;
      if (p.contains("name")) e.name = p["name"].get<std::string>();
      if (p.contains("profile")) e.profile = p["profile"].get<std::string>();
      if (p.contains("cred_mode")) e.cred_mode = parseCredMode(p["cred_mode"].get<std::string>());
      if (p.contains("weight")) e.weight = p["weight"].get<int>();
      if (p.contains("wallet")) e.wallet = p["wallet"].get<std::string>();
      if (p.contains("account")) e.account = p["account"].get<std::string>();
      if (p.contains("worker")) e.worker = p["worker"].get<std::string>();
      if (p.contains("password")) e.password = p["password"].get<std::string>();
      if (p.contains("endpoints") && p["endpoints"].is_array()) {
        for (const auto& ep : p["endpoints"]) {
          Endpoint ed;
          if (ep.contains("host")) ed.host = ep["host"].get<std::string>();
          if (ep.contains("port")) ed.port = static_cast<uint16_t>(ep["port"].get<int>());
          if (ep.contains("use_tls")) ed.use_tls = ep["use_tls"].get<bool>();
          e.endpoints.push_back(ed);
        }
      }
      if (p.contains("rpc") && p["rpc"].is_object()) {
        PoolEntry::RpcConfig rc;
        const auto& r = p["rpc"];
        if (r.contains("url")) rc.url = r["url"].get<std::string>();
        if (r.contains("use_tls")) rc.use_tls = r["use_tls"].get<bool>();
        if (r.contains("auth")) rc.auth = r["auth"].get<std::string>();
        if (r.contains("username")) rc.username = r["username"].get<std::string>();
        if (r.contains("password")) rc.password = r["password"].get<std::string>();
        if (r.contains("cookie_path")) rc.cookie_path = r["cookie_path"].get<std::string>();
        e.rpc = rc;
      }
      if (p.contains("gbt") && p["gbt"].is_object()) {
        PoolEntry::GbtConfig gc;
        const auto& g = p["gbt"];
        if (g.contains("poll_ms")) gc.poll_ms = g["poll_ms"].get<int>();
        if (g.contains("rules") && g["rules"].is_array()) {
          for (const auto& r : g["rules"]) gc.rules.push_back(r.get<std::string>());
        }
        if (g.contains("cb_tag")) gc.cb_tag = g["cb_tag"].get<std::string>();
        if (g.contains("allow_synth_coinbase")) gc.allow_synth_coinbase = g["allow_synth_coinbase"].get<bool>();
        if (g.contains("payout_script_hex")) gc.payout_script_hex = g["payout_script_hex"].get<std::string>();
        e.gbt = gc;
      }
      if (p.contains("policy") && p["policy"].is_object()) {
        const auto& po = p["policy"];
        if (po.contains("force_clean_jobs")) e.policy.force_clean_jobs = po["force_clean_jobs"].get<bool>();
        if (po.contains("clean_jobs_default")) e.policy.clean_jobs_default = po["clean_jobs_default"].get<bool>();
        if (po.contains("version_mask")) { try { e.policy.version_mask = std::stoul(po["version_mask"].get<std::string>(), nullptr, 16); } catch(...){} }
        if (po.contains("ntime_min")) { try { e.policy.ntime_min = std::stoul(po["ntime_min"].get<std::string>(), nullptr, 16); } catch(...){} }
        if (po.contains("ntime_max")) { try { e.policy.ntime_max = std::stoul(po["ntime_max"].get<std::string>(), nullptr, 16); } catch(...){} }
        if (po.contains("share_nbits")) { try { e.policy.share_nbits = std::stoul(po["share_nbits"].get<std::string>(), nullptr, 16); } catch(...){} }
      }
      cfg.pools.push_back(std::move(e));
    }
  }
  return cfg;
}

AppConfig loadDefault() {
  AppConfig cfg;
  PoolEntry e;
  e.name = "dummy";
  e.profile = "generic";
  e.cred_mode = CredMode::WalletAsUser;
  e.wallet = "bc1qexample";
  e.worker = "w";
  e.password = "x";
  Endpoint ep; ep.host = "localhost"; ep.port = 3333; ep.use_tls = false; e.endpoints.push_back(ep);
  cfg.pools.push_back(e);
  return cfg;
}

}


