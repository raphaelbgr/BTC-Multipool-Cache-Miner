#include <atomic>
#include <chrono>
#include <csignal>
#include <iostream>
#include <string>
#include <thread>
#include <nlohmann/json.hpp>

#include "adapters/pool_profiles.h"
#include "adapters/stratum_adapter.h"
#include "adapters/stratum_runner.h"
#include "net/stratum_client.h"
#include "registry/work_source_registry.h"
#include "submit/stratum_submitter.h"

static std::atomic<bool> g_stop{false};

static void on_sigint(int) { g_stop.store(true); }

int main(int argc, char** argv) {
  if (argc < 3) {
    std::cerr << "Usage: stratum_registry_runner <host:port> <wallet> [worker] [pass] [profile]\n";
    return 1;
  }
  std::signal(SIGINT, on_sigint);

  std::string hostport = argv[1];
  std::string wallet = argv[2];
  std::string worker = (argc >= 4) ? argv[3] : "bench";
  std::string pass = (argc >= 5) ? argv[4] : "x";
  adapters::PoolProfile profile = adapters::PoolProfile::kGeneric;
  if (argc >= 6) {
    std::string prof = argv[5];
    if (prof == "viabtc") profile = adapters::PoolProfile::kViaBTC;
    else if (prof == "f2pool") profile = adapters::PoolProfile::kF2Pool;
  }
  auto creds = adapters::formatStratumCredentials(profile, wallet, worker, pass);
  std::string user = creds.first;
  pass = creds.second;

  auto pos = hostport.find(":");
  if (pos == std::string::npos) {
    std::cerr << "Invalid host:port\n";
    return 1;
  }
  std::string host = hostport.substr(0, pos);
  uint16_t port = static_cast<uint16_t>(std::stoi(hostport.substr(pos + 1)));

  // Registry with one slot
  registry::WorkSourceRegistry reg(1);
  adapters::StratumAdapter adapter("stratum");
  adapters::StratumRunner runner(&adapter, host, port, user, pass);
  runner.start();
  submit::StratumSubmitter submitter(&runner, user);

  uint64_t last_gen = 0;
  std::cout << "Press 's' then Enter to manually submit a test share (dummy extranonce2/ntime/nonce)" << std::endl;
  while (!g_stop.load()) {
    // Drain full results and set registry
    while (true) {
      auto full = adapter.pollNormalizedFull();
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
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
  }

  runner.stop();
  return 0;
}


