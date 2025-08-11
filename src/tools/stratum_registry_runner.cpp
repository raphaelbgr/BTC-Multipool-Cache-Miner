#include <atomic>
#include <chrono>
#include <csignal>
#include <iostream>
#include <string>
#include <thread>
#include <nlohmann/json.hpp>
#include <queue>
#include <mutex>
#include <cstdlib>
#include <sstream>

#include "adapters/pool_profiles.h"
#include "adapters/stratum_adapter.h"
#include "adapters/stratum_runner.h"
#include "adapters/gbt_adapter.h"
#include "adapters/gbt_runner.h"
#include "net/stratum_client.h"
#include "registry/work_source_registry.h"
#include "submit/stratum_submitter.h"
#include "config/config.h"

static std::atomic<bool> g_stop{false};

static void on_sigint(int) { g_stop.store(true); }

int main(int argc, char** argv) {
  std::signal(SIGINT, on_sigint);
  std::string host;
  uint16_t port = 0;
  bool use_tls = false;
  std::string user;
  std::string pass = "x";
  adapters::PoolProfile profile = adapters::PoolProfile::kGeneric;

  // Mode A: legacy CLI args
  bool used_legacy = false;
  if (argc >= 3) {
    used_legacy = true;
    std::string hostport = argv[1];
    std::string walletOrAccount = argv[2];
    std::string worker = (argc >= 4) ? argv[3] : "bench";
    pass = (argc >= 5) ? argv[4] : "x";
    if (argc >= 6) {
      std::string prof = argv[5];
      if (prof == "viabtc") profile = adapters::PoolProfile::kViaBTC;
      else if (prof == "f2pool") profile = adapters::PoolProfile::kF2Pool;
      else profile = adapters::PoolProfile::kGeneric;
    }
    auto creds = adapters::formatStratumCredentials(profile, walletOrAccount, worker, pass);
    user = creds.first; pass = creds.second;
    auto pos = hostport.find(":");
    if (pos == std::string::npos) {
      std::cerr << "Invalid host:port\n";
      return 1;
    }
    host = hostport.substr(0, pos);
    port = static_cast<uint16_t>(std::stoi(hostport.substr(pos + 1)));
    // Heuristic: TLS for 443/3334
    use_tls = (port == 443 || port == 3334);
  }

  // Mode B: config-driven when no args
  if (!used_legacy) {
    auto cfg = config::loadFromJsonFile("config/pools.json");
    if (cfg.pools.empty()) {
      std::cerr << "No config found at config/pools.json and no CLI args provided.\n";
      return 1;
    }
    // Choose pool by env BMAD_POOL or first
    std::string choice;
    if (const char* env = std::getenv("BMAD_POOL")) choice = env;
    const config::PoolEntry* sel = nullptr;
    for (const auto& p : cfg.pools) {
      if (choice.empty() || p.name == choice) { sel = &p; if (!choice.empty()) break; }
    }
    if (!sel) sel = &cfg.pools.front();
    if (sel->endpoints.empty()) {
      std::cerr << "Selected pool has no endpoints in config.\n";
      return 1;
    }
    // Rotation policy: try endpoints in listed order; on N failures/quick disconnects, advance.
    size_t ep_index = 0;
    auto select_ep = [&](size_t idx){
      host = sel->endpoints[idx].host; port = sel->endpoints[idx].port; use_tls = sel->endpoints[idx].use_tls; };
    select_ep(ep_index);
    // Profile mapping for legacy formatter (only affects minor defaults)
    if (sel->profile == "viabtc") profile = adapters::PoolProfile::kViaBTC;
    else if (sel->profile == "f2pool") profile = adapters::PoolProfile::kF2Pool;
    else profile = adapters::PoolProfile::kGeneric;
    // Build username
    std::string walletOrAccount;
    std::string worker;
    if (sel->cred_mode == config::CredMode::WalletAsUser) {
      walletOrAccount = sel->wallet;
      worker = sel->worker;
    } else {
      walletOrAccount = sel->account;
      worker = sel->worker;
    }
    pass = sel->password.empty() ? std::string("x") : sel->password;
    auto creds = adapters::formatStratumCredentials(profile, walletOrAccount, worker, pass);
    user = creds.first; pass = creds.second;
    nlohmann::json j; j["pool_name"] = sel->name; j["host"] = host; j["port"] = port; j["tls"] = use_tls; j["cred_mode"] = (sel->cred_mode==config::CredMode::WalletAsUser?"wallet_as_user":"account_worker"); j["user_preview"] = user.substr(0, user.find_last_of('.')+1) + "***";
    std::cout << j.dump() << std::endl;
  }

  // Registry with one slot
  registry::WorkSourceRegistry reg(1);
  adapters::StratumAdapter adapter("stratum");
  adapters::StratumRunner* stratum_runner_ptr = nullptr;
  std::unique_ptr<adapters::StratumRunner> stratum_runner;
  std::unique_ptr<adapters::GbtAdapter> gbt_adapter;
  std::unique_ptr<adapters::GbtRunner> gbt_runner;

  // If selected profile is GBT, start GBT runner; else Stratum
  bool use_gbt = false;
  if (!used_legacy) {
    auto cfg = config::loadFromJsonFile("config/pools.json");
    std::string choice; if (const char* env = std::getenv("BMAD_POOL")) choice = env;
    const config::PoolEntry* sel = nullptr;
    for (const auto& p : cfg.pools) { if (choice.empty() || p.name == choice) { sel = &p; if (!choice.empty()) break; } }
    if (sel && sel->profile == "gbt") {
      use_gbt = true;
      gbt_adapter = std::make_unique<adapters::GbtAdapter>("gbt");
      gbt_runner = std::make_unique<adapters::GbtRunner>(gbt_adapter.get(), *sel);
      gbt_runner->start();
    }
  }
  if (!use_gbt) {
    stratum_runner = std::make_unique<adapters::StratumRunner>(&adapter, host, port, user, pass, use_tls);
    stratum_runner_ptr = stratum_runner.get();
    stratum_runner->start();
  }
  submit::StratumSubmitter submitter(stratum_runner_ptr, user);

  uint64_t last_gen = 0;
  std::string last_ntime_hex;
  // Optional env to auto-submit once on first job: BMAD_AUTO_SUBMIT=en2:ntime:nonce (hex)
  std::string auto_en2, auto_ntime, auto_nonce;
  bool auto_submit = false;
  if (const char* env = std::getenv("BMAD_AUTO_SUBMIT")) {
    std::string s(env);
    auto p1 = s.find(':');
    auto p2 = s.find(':', p1 == std::string::npos ? 0 : p1 + 1);
    if (p1 != std::string::npos && p2 != std::string::npos) {
      auto_en2 = s.substr(0, p1);
      auto_ntime = s.substr(p1 + 1, p2 - p1 - 1);
      auto_nonce = s.substr(p2 + 1);
      auto_submit = true;
    }
  }
  bool auto_submitted = false;
  std::cout << "Type: s [<en2_hex> <ntime_hex> <nonce_hex>] or submit <en2_hex> <ntime_hex> <nonce_hex>" << std::endl;

  // Non-blocking stdin reader thread feeding a queue
  std::queue<std::string> input_queue;
  std::mutex input_mu;
  std::thread input_thread([&]{
    std::string line;
    while (!g_stop.load()) {
      if (!std::getline(std::cin, line)) {
        // If stdin closed, exit thread
        break;
      }
      {
        std::lock_guard<std::mutex> lk(input_mu);
        input_queue.push(line);
      }
    }
  });
  input_thread.detach();
  while (!g_stop.load()) {
    // Drain full results and set registry
    while (true) {
      std::optional<normalize::NormalizerResult> full;
      if (use_gbt) full = gbt_adapter->pollNormalizedFull();
      else full = adapter.pollNormalizedFull();
      if (!full.has_value()) break;
      reg.set(0, full->item, full->job_const);
    }

    auto snap = reg.get(0);
    if (snap.has_value() && snap->gen != last_gen) {
      last_gen = snap->gen;
      nlohmann::json j;
      j["gen"] = last_gen;
      j["work_id"] = snap->item.work_id;
      j["nbits_hex"] = snap->item.nbits;
      j["ntime"] = snap->item.ntime;
      j["clean_jobs"] = snap->item.clean_jobs;
      std::cout << j.dump() << std::endl;
      // cache last seen ntime as hex for convenience
      {
        std::ostringstream oss; oss << std::hex << std::nouppercase << snap->item.ntime;
        last_ntime_hex = oss.str();
      }
      if (auto_submit && !auto_submitted) {
        bool sent = submitter.submitShareHex(auto_en2, auto_ntime, auto_nonce);
        nlohmann::json js; js["auto_submit"] = sent ? "submitted" : "submit_failed";
        js["en2"] = auto_en2; js["ntime"] = auto_ntime; js["nonce"] = auto_nonce;
        std::cout << js.dump() << std::endl;
        auto_submitted = true;
      }
    }
    // Process any queued stdin lines without blocking
    {
      std::string line;
      bool had_line = false;
      {
        std::lock_guard<std::mutex> lk(input_mu);
        if (!input_queue.empty()) { line = std::move(input_queue.front()); input_queue.pop(); had_line = true; }
      }
      if (had_line) {
        std::istringstream iss(line);
        std::string cmd, en2, ntime_hex, nonce_hex;
        if (iss >> cmd) {
          if (cmd == "s" || cmd == "submit") {
            // Accept with or without args: if missing, use auto or defaults
            bool haveArgs = static_cast<bool>(iss >> en2 >> ntime_hex >> nonce_hex);
            if (!haveArgs) {
              en2 = !auto_en2.empty() ? auto_en2 : std::string("00000000");
              ntime_hex = !auto_ntime.empty() ? auto_ntime : (last_ntime_hex.empty() ? std::string("00000000") : last_ntime_hex);
              nonce_hex = !auto_nonce.empty() ? auto_nonce : std::string("00000000");
            }
            bool sent = submitter.submitShareHex(en2, ntime_hex, nonce_hex);
            nlohmann::json j; j["msg"] = sent ? "submitted" : "submit_failed";
            j["en2"] = en2; j["ntime"] = ntime_hex; j["nonce"] = nonce_hex;
            if (!haveArgs && last_ntime_hex.empty()) j["note"] = "no job yet; submit likely fails until first notify";
            std::cout << j.dump() << std::endl;
          } else {
            std::cout << "{\"error\":\"unknown command\"}" << std::endl;
          }
        }
      }
    }
    // Passive rotation: advance endpoint if repeated failures/disconnects
    if (!used_legacy && !use_gbt) {
      if (stratum_runner->connectFailures() >= 3 || stratum_runner->quickDisconnects() >= 3) {
        stratum_runner->stop();
        stratum_runner->resetCounters();
        // advance to next endpoint
        auto cfg = config::loadFromJsonFile("config/pools.json");
        // re-find selected pool and bump index
        const config::PoolEntry* sel = nullptr; std::string choice; if (const char* env = std::getenv("BMAD_POOL")) choice = env;
        for (const auto& p : cfg.pools) { if (choice.empty() || p.name == choice) { sel = &p; if (!choice.empty()) break; } }
        if (sel && !sel->endpoints.empty()) {
          // pick next different endpoint
          size_t cur = 0; for (; cur < sel->endpoints.size(); ++cur) { if (sel->endpoints[cur].host == host && sel->endpoints[cur].port == port) break; }
          size_t next = (cur + 1) % sel->endpoints.size();
          host = sel->endpoints[next].host; port = sel->endpoints[next].port; use_tls = sel->endpoints[next].use_tls;
          // reinitialize runner in-place by constructing a new one on the same storage
          stratum_runner.reset();
          stratum_runner = std::make_unique<adapters::StratumRunner>(&adapter, host, port, user, pass, use_tls);
          stratum_runner->start();
        }
      }
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
  }

  if (use_gbt) { if (gbt_runner) gbt_runner->stop(); }
  else { if (stratum_runner) stratum_runner->stop(); }
  return 0;
}


