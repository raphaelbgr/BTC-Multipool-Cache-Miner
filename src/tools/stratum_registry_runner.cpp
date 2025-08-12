#include <atomic>
#include <chrono>
#include <csignal>
#include <iostream>
#include <fstream>
#include <string>
#include <thread>
#include <nlohmann/json.hpp>
#include <queue>
#include <mutex>
#include <unordered_map>
#include <algorithm>
#include <cstdlib>
#include <sstream>
#include <memory>

#include "adapters/pool_profiles.h"
#include "adapters/stratum_adapter.h"
#include "adapters/stratum_runner.h"
#include "adapters/gbt_adapter.h"
#include "adapters/gbt_runner.h"
#include "net/stratum_client.h"
#include "registry/work_source_registry.h"
#include "cuda/engine.h"
#include "cuda/launch_plan.h"
#include "submit/stratum_submitter.h"
#include "submit/submit_router.h"
#include "submit/cpu_verify.h"
#include "config/config.h"
#include "cuda/hit_ring.h"
#include "cuda/engine.h"
#include "scheduler/scheduler.h"
#include "normalize/endianness.h"
#include "store/outbox.h"

static std::atomic<bool> g_stop{false};

static void on_sigint(int) { g_stop.store(true); }

int main(int argc, char** argv) {
  // Simple scheduler state: per-source weights and a tick for fair selection
  scheduler::SchedulerState scheduler;
  // Load scheduler source weights from config (by pool index mapping to source_id)
  auto cfg_weights = config::loadFromJsonFile("config/pools.json");
  scheduler.max_weight_cap = static_cast<uint32_t>(std::max(1, cfg_weights.scheduler.max_weight));
  for (size_t sid = 0; sid < cfg_weights.pools.size(); ++sid) {
    int w = cfg_weights.pools[sid].weight;
    if (w <= 0) w = 1;
    scheduler.source_weight[static_cast<uint32_t>(sid)] = static_cast<uint32_t>(w);
  }

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

  // Registry with multiple slots (supports multiple sources)
  auto cfg_all = config::loadFromJsonFile("config/pools.json");
  const std::size_t num_slots = std::max<std::size_t>(1, cfg_all.pools.size());
  registry::WorkSourceRegistry reg(num_slots);
  // Multi-source structures
  enum class SourceKind { Stratum, Gbt };
  struct SourceInfo {
    SourceKind kind{SourceKind::Stratum};
    adapters::StratumAdapter* stratum_adapter{nullptr};
    adapters::StratumRunner* stratum_runner{nullptr};
    submit::StratumSubmitter* stratum_submitter{nullptr};
    adapters::GbtAdapter* gbt_adapter{nullptr};
  };
  std::vector<SourceInfo> sources(num_slots);
  std::vector<std::unique_ptr<adapters::StratumAdapter>> strat_adapters;
  std::vector<std::unique_ptr<adapters::StratumRunner>> strat_runners;
  std::vector<std::unique_ptr<submit::StratumSubmitter>> strat_submitters;
  std::unique_ptr<adapters::GbtAdapter> gbt_adapter;
  std::unique_ptr<adapters::GbtRunner> gbt_runner;
  std::unique_ptr<submit::GbtSubmitter> gbt_submitter;
  std::unique_ptr<submit::SubmitRouter> submit_router;
  // Map from work_id to source slot
  std::unordered_map<uint64_t, uint32_t> workid_to_source;
  store::Outbox outbox;
  const std::string outbox_path = "logs/outbox.bin";
  outbox.loadFromFile(outbox_path);
  // On startup, attempt to drain and resubmit old pending hits
  auto replay_pending = [&](submit::SubmitRouter* router){
    if (!router) return;
    int replayed = 0;
    while (true) {
      auto s = outbox.tryDequeue();
      if (!s.has_value()) break;
      // Re-verify locally before resubmitting
      // The target is unknown here, so we optimistically resubmit; pools enforce validity
      submit::HitRecord rec{}; rec.work_id = s->work_id; rec.nonce = s->nonce; std::memcpy(rec.header80, s->header80, 80);
      // Route through router; outbox enqueue is idempotent via dedupe
      router->routeRaw(rec);
      ++replayed;
      if (replayed >= 128) break; // avoid long stalls on startup
    }
  };

  // If selected profile is GBT, start GBT runner; else Stratum
  bool use_gbt = false;
  if (!used_legacy) {
    // Initialize sources per pool
    for (std::size_t idx = 0; idx < cfg_all.pools.size(); ++idx) {
      const auto& p = cfg_all.pools[idx];
      if (p.profile == "gbt") {
        if (!gbt_adapter) {
          use_gbt = true;
          gbt_adapter = std::make_unique<adapters::GbtAdapter>("gbt");
          gbt_runner = std::make_unique<adapters::GbtRunner>(gbt_adapter.get(), p);
          gbt_runner->start();
          // Initialize GBT submitter if RPC configured
          if (p.rpc.has_value()) gbt_submitter = std::make_unique<submit::GbtSubmitter>(*p.rpc);
        }
        sources[idx].kind = SourceKind::Gbt;
        sources[idx].gbt_adapter = gbt_adapter.get();
      } else {
        // Create stratum adapter and runner for this slot
        auto sad = std::make_unique<adapters::StratumAdapter>("stratum");
        auto& adp = *sad;
        // Apply policy
        adp.setCleanJobs(p.policy.clean_jobs_default);
        adp.forceCleanJobs(p.policy.force_clean_jobs);
        if (p.policy.version_mask.has_value()) adp.setVersionMask(*p.policy.version_mask);
        if (p.policy.ntime_min.has_value() || p.policy.ntime_max.has_value()) {
          adp.setNtimeCaps(p.policy.ntime_min.value_or(0), p.policy.ntime_max.value_or(0));
        }
        if (p.policy.share_nbits.has_value()) adp.updateVarDiff(*p.policy.share_nbits);
        // Endpoint
        if (p.endpoints.empty()) continue;
        std::string h = p.endpoints[0].host; uint16_t prt = p.endpoints[0].port; bool tls = p.endpoints[0].use_tls;
        // Credentials
        adapters::PoolProfile prof = adapters::PoolProfile::kGeneric;
        if (p.profile == "viabtc") prof = adapters::PoolProfile::kViaBTC; else if (p.profile == "f2pool") prof = adapters::PoolProfile::kF2Pool;
        std::string walletOrAccount = (p.cred_mode == config::CredMode::WalletAsUser) ? p.wallet : p.account;
        std::string workerName = p.worker;
        std::string pwd = p.password.empty()? std::string("x") : p.password;
        auto creds = adapters::formatStratumCredentials(prof, walletOrAccount, workerName, pwd);
        auto srun = std::make_unique<adapters::StratumRunner>(&adp, h, prt, creds.first, creds.second, tls);
        srun->start();
        auto subm = std::make_unique<submit::StratumSubmitter>(srun.get(), creds.first);
        sources[idx].kind = SourceKind::Stratum;
        sources[idx].stratum_adapter = &adp;
        sources[idx].stratum_runner = srun.get();
        sources[idx].stratum_submitter = subm.get();
        strat_submitters.push_back(std::move(subm));
        strat_runners.push_back(std::move(srun));
        strat_adapters.push_back(std::move(sad));
      }
    }
    // Submit router that routes shares to the originating stratum source
    submit_router = std::make_unique<submit::SubmitRouter>([&](const submit::HitRecord& rec){
      auto it = workid_to_source.find(rec.work_id);
      if (it != workid_to_source.end()) {
        uint32_t sid = it->second;
        if (sid < sources.size() && sources[sid].kind == SourceKind::Stratum && sources[sid].stratum_submitter && sources[sid].stratum_adapter) {
          // Build extranonce2 hex as zeros of configured size (placeholder)
          int en2_size = static_cast<int>(sources[sid].stratum_adapter->extranonce2Size());
          std::string en2_hex; en2_hex.assign(static_cast<size_t>(en2_size) * 2, '0');
          (void)sources[sid].stratum_submitter->submitFromHeader(rec.header80, en2_hex);
        }
      }
      store::PendingSubmit ps{}; ps.work_id = rec.work_id; ps.nonce = rec.nonce; std::memcpy(ps.header80, rec.header80, 80);
      outbox.appendToFile(outbox_path, ps);
    });
    submit_router->attachOutbox(&outbox);
    replay_pending(submit_router.get());
  }
  // Dummy for legacy interactive submit path; will use first stratum if present
  adapters::StratumRunner* stratum_runner_ptr = strat_runners.empty() ? nullptr : strat_runners.front().get();
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
  if (use_gbt) {
    std::cout << "Type: submit_header <header80_hex> (GBT)" << std::endl;
  }

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
  // Initialize device hit buffer from config
  {
    auto cfg = config::loadFromJsonFile("config/pools.json");
    int cap = std::max(1, cfg.cuda.hit_ring_capacity);
    cuda_engine::initDeviceHitBuffer(static_cast<uint32_t>(cap));
  }
  uint32_t nonce_base = 0; // same nonce across jobs per iteration
  // Metrics
  obs::MetricsRegistry metrics;
  // Metrics sink config (load once)
  auto mcfg = config::loadFromJsonFile("config/pools.json");
  const int metrics_interval_ms = std::max(100, mcfg.metrics.dump_interval_ms);
  const bool metrics_file = mcfg.metrics.enable_file;
  const std::string metrics_path = mcfg.metrics.file_path;
  std::unique_ptr<std::ofstream> metrics_ofs;
  if (metrics_file) {
    metrics_ofs = std::make_unique<std::ofstream>(metrics_path, std::ios::out | std::ios::app);
  }
  // Optional HTTP metrics server (placeholder, starts background thread)
  std::unique_ptr<obs::MetricsHttpServer> metrics_http;
  if (mcfg.metrics.enable_http) {
    metrics_http = std::make_unique<obs::MetricsHttpServer>(&metrics, mcfg.metrics.http_host.c_str(), mcfg.metrics.http_port);
    metrics_http->start();
  }
  // Per-source backpressure tracking
  std::unordered_map<uint32_t, int> last_acc, last_rej;
  std::unordered_map<uint32_t, int> src_penalty;
  const int latency_penalty_ms = std::max(0, cfg_all.scheduler.latency_penalty_ms);
  uint64_t last_backpressure_ms = 0;
  // Auto-tuning knobs
  int desired_threads_per_job = std::max(1, cfg_all.cuda.desired_threads_per_job);
  int nonces_per_thread = std::max(1, cfg_all.cuda.nonces_per_thread);
  const int budget_ms = 16; // aim for ~16ms batches by default
  // Report basic CUDA memory info
  {
    uint64_t free_b=0,total_b=0; if (cuda_engine::getDeviceMemoryInfo(&free_b,&total_b)) {
      metrics.setGauge("cuda.mem_free_mb", static_cast<int64_t>(free_b/1024/1024));
      metrics.setGauge("cuda.mem_total_mb", static_cast<int64_t>(total_b/1024/1024));
    }
  }

  while (!g_stop.load()) {
    // Drain full results and set registry
    // Poll all stratum adapters and optional GBT into their slots
    for (std::size_t sid = 0; sid < sources.size(); ++sid) {
      bool progressed = false;
      if (sources[sid].kind == SourceKind::Stratum && sources[sid].stratum_adapter) {
        while (true) {
          auto full = sources[sid].stratum_adapter->pollNormalizedFull();
          if (!full.has_value()) break;
          auto item = full->item; item.source_id = static_cast<uint32_t>(sid); item.active = true;
          workid_to_source[item.work_id] = static_cast<uint32_t>(sid);
          reg.set(sid, item, full->job_const);
          progressed = true;
        }
      } else if (sources[sid].kind == SourceKind::Gbt && sources[sid].gbt_adapter) {
        while (true) {
          auto full = sources[sid].gbt_adapter->pollNormalizedFull();
          if (!full.has_value()) break;
          auto item = full->item; item.source_id = static_cast<uint32_t>(sid); item.active = true;
          workid_to_source[item.work_id] = static_cast<uint32_t>(sid);
          reg.set(sid, item, full->job_const);
          progressed = true;
        }
      }
      (void)progressed;
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

    // Build active job list across all slots for fair iteration
    std::vector<uint64_t> active_ids;
    std::unordered_map<uint64_t, registry::WorkSlotSnapshot> id_to_snap;
    for (std::size_t i = 0; i < reg.size(); ++i) {
      auto s = reg.get(i);
      if (!s.has_value()) continue;
      if (!s->item.active) continue;
      active_ids.push_back(s->item.work_id);
      id_to_snap.emplace(s->item.work_id, *s);
    }
    if (!active_ids.empty()) {
      // Prepare DeviceJob table (scaffold) for the active set
      std::vector<cuda_engine::DeviceJob> jobs;
      jobs.reserve(active_ids.size());
      for (auto wid : active_ids) {
        const auto& ss = id_to_snap[wid];
        cuda_engine::DeviceJob dj{};
        dj.version = ss.item.version;
        dj.ntime = ss.item.ntime;
        dj.nbits = ss.item.nbits;
        dj.vmask = ss.item.vmask;
        dj.ntime_min = ss.item.ntime_min;
        dj.ntime_max = ss.item.ntime_max;
        dj.extranonce2_size = ss.item.extranonce2_size;
        dj.work_id = ss.item.work_id;
        std::memcpy(dj.prevhash_le, ss.item.prevhash_le.data(), sizeof(dj.prevhash_le));
        std::memcpy(dj.merkle_root_le, ss.item.merkle_root_le.data(), sizeof(dj.merkle_root_le));
        std::memcpy(dj.share_target_le, ss.item.share_target_le.data(), sizeof(dj.share_target_le));
        // Precompute big-endian share target for device compare
        for (int w = 0; w < 8; ++w) {
          uint32_t t = ss.item.share_target_le[7 - w];
          dj.share_target_be[w*4+0] = static_cast<uint8_t>((t >> 24) & 0xFF);
          dj.share_target_be[w*4+1] = static_cast<uint8_t>((t >> 16) & 0xFF);
          dj.share_target_be[w*4+2] = static_cast<uint8_t>((t >> 8) & 0xFF);
          dj.share_target_be[w*4+3] = static_cast<uint8_t>((t) & 0xFF);
        }
        std::memcpy(dj.block_target_le, ss.item.block_target_le.data(), sizeof(dj.block_target_le));
        std::memcpy(dj.midstate_le, ss.job_const.midstate_le.data(), sizeof(dj.midstate_le));
        jobs.push_back(dj);
      }
      cuda_engine::uploadDeviceJobs(jobs.data(), static_cast<uint32_t>(jobs.size()));
      // Apply scheduler weighting with per-source backpressure
      auto now_bp_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::steady_clock::now().time_since_epoch()).count();
      if (now_bp_ms - last_backpressure_ms > 2000) {
        for (uint32_t sid = 0; sid < sources.size(); ++sid) {
          int p = 0;
          if (sources[sid].kind == SourceKind::Stratum && sources[sid].stratum_runner) {
            int acc = sources[sid].stratum_runner->acceptedSubmits();
            int rej = sources[sid].stratum_runner->rejectedSubmits();
            int lat = sources[sid].stratum_runner->avgSubmitMs();
            int dacc = acc - last_acc[sid];
            int drej = rej - last_rej[sid];
            last_acc[sid] = acc; last_rej[sid] = rej;
            if (drej > dacc) p += 1;              // recent rejects > accepts
            if (latency_penalty_ms > 0 && lat > latency_penalty_ms) p += 1; // slow submits
          }
          // Decay previous penalty if conditions improved
          int prev = src_penalty[sid];
          if (p == 0 && prev > 0) prev -= 1; else if (p > 0) prev = std::min(prev + p, 3);
          src_penalty[sid] = prev;
          scheduler.source_penalty[sid] = static_cast<uint32_t>(prev);
        }
        last_backpressure_ms = now_bp_ms;
      }
      auto selected_ids = scheduler.select(active_ids, id_to_snap, 64);
      // Mining stub using uploaded jobs (same nonce across jobs) and measure kernel time
      auto t0 = std::chrono::steady_clock::now();
      // Use configured desired_threads_per_job to compute a plan (auto-tuned)
      auto plan = cuda_engine::computeLaunchPlan(static_cast<uint32_t>(jobs.size()),
                                                 static_cast<uint64_t>(desired_threads_per_job));
      if (plan.num_jobs > 0 && plan.blocks_per_job > 0 && plan.threads_per_block > 0) {
        if (nonces_per_thread > 1) {
          cuda_engine::launchMineWithPlanBatch(plan.num_jobs, plan.blocks_per_job, plan.threads_per_block, nonce_base, static_cast<uint32_t>(nonces_per_thread));
        } else {
          cuda_engine::launchMineWithPlan(plan.num_jobs, plan.blocks_per_job, plan.threads_per_block, nonce_base);
        }
      } else {
        cuda_engine::launchMineStub(static_cast<uint32_t>(jobs.size()), nonce_base);
      }
      auto t1 = std::chrono::steady_clock::now();
      int kernel_ms = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count());
      metrics.setGauge("cuda.kernel_ms", kernel_ms);
      metrics.setGauge("cuda.jobs", static_cast<int64_t>(jobs.size()));
      // Auto-tune nonces_per_thread if far from budget
      {
        cuda_engine::AutoTuneInputs ai; ai.observedMsPerBatch = static_cast<uint32_t>(kernel_ms);
        ai.maxMsPerBatch = static_cast<uint32_t>(budget_ms);
        ai.currentMicroBatch = static_cast<uint32_t>(nonces_per_thread);
        auto dec = cuda_engine::autoTuneMicroBatch(ai);
        nonces_per_thread = static_cast<int>(dec.nextMicroBatch);
      }
      nonce_base += 1; // advance to keep testing new nonce across all jobs
      cuda_engine::HitRecord out[64]; uint32_t n = 0;
      if (submit_router && cuda_engine::drainDeviceHits(out, 64, &n) && n > 0) {
        metrics.increment("device.hits", n);
        for (uint32_t i = 0; i < n; ++i) {
          auto it = id_to_snap.find(out[i].work_id);
          if (it == id_to_snap.end()) continue;
          const auto& ss = it->second;
          uint8_t header[80] = {0};
          // Assemble 80-byte header in big-endian fields for local CPU verify
          auto store_u32_be = [](uint8_t* p, uint32_t v){ p[0] = static_cast<uint8_t>((v >> 24) & 0xFF); p[1] = static_cast<uint8_t>((v >> 16) & 0xFF); p[2] = static_cast<uint8_t>((v >> 8) & 0xFF); p[3] = static_cast<uint8_t>(v & 0xFF); };
          store_u32_be(&header[0],  ss.item.version);
          {
            uint8_t be_prev[32];
            normalize::leU32WordsToBe32Bytes(ss.item.prevhash_le, be_prev);
            std::memcpy(&header[4], be_prev, 32);
          }
          {
            uint8_t be_mr[32];
            normalize::leU32WordsToBe32Bytes(ss.item.merkle_root_le, be_mr);
            std::memcpy(&header[36], be_mr, 32);
          }
          store_u32_be(&header[68], ss.item.ntime);
          store_u32_be(&header[72], ss.item.nbits);
          store_u32_be(&header[76], out[i].nonce);
          // Classify block hit (CPU hash <= block target) for logging/routing policies
          uint8_t h1[32]; submit::sha256(header, 80, h1);
          uint8_t h2[32]; submit::sha256(h1, 32, h2);
          bool is_block = submit::isHashLEQTarget(h2, ss.item.block_target_le);
          if (is_block && gbt_submitter) {
            // Prefer solo (GBT) for block hits
            gbt_submitter->submitHeader(header);
            metrics.increment("submit.blocks_gbt", 1);
          } else {
            submit_router->verifyAndSubmit(header, ss.item.share_target_le, out[i].work_id, out[i].nonce);
            metrics.increment("submit.shares", 1);
          }
        }
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
          } else if (use_gbt && cmd == "submit_header") {
            std::string hhex;
            if (!(iss >> hhex) || hhex.size() != 160) {
              std::cout << "{\"error\":\"need 80-byte header hex\"}" << std::endl;
            } else {
              uint8_t header[80];
              for (int i = 0; i < 80; ++i) {
                unsigned int v; sscanf(hhex.c_str() + 2*i, "%02x", &v); header[i] = static_cast<uint8_t>(v);
              }
              auto snap = reg.get(0);
              if (snap.has_value()) {
                // Extract nonce from last 4 bytes (BE)
                uint32_t nonce = (header[76] << 24) | (header[77] << 16) | (header[78] << 8) | header[79];
                bool ok = submit_router && submit_router->verifyAndSubmit(header, snap->item.share_target_le, snap->item.work_id, nonce);
                nlohmann::json js; js["submit_header"] = ok ? "accepted_local" : "rejected_local"; std::cout << js.dump() << std::endl;
              } else {
                std::cout << "{\"error\":\"no job yet\"}" << std::endl;
              }
            }
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

    // Periodically set stratum metrics and dump metrics snapshot
    static uint64_t last_metrics_ms = 0;
    auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    if (now_ms - last_metrics_ms > metrics_interval_ms) {
      if (stratum_runner_ptr) {
        metrics.setGauge("stratum.avg_submit_ms", stratum_runner_ptr->avgSubmitMs());
        metrics.setGauge("stratum.accepted", stratum_runner_ptr->acceptedSubmits());
        metrics.setGauge("stratum.rejected", stratum_runner_ptr->rejectedSubmits());
      }
      const auto mjson = metrics.snapshot();
      if (metrics_ofs && metrics_ofs->good()) {
        (*metrics_ofs) << mjson.dump() << "\n";
        metrics_ofs->flush();
      } else {
        std::cout << mjson.dump() << std::endl;
      }
      last_metrics_ms = now_ms;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
  }

  if (use_gbt) { if (gbt_runner) gbt_runner->stop(); }
  else { if (stratum_runner) stratum_runner->stop(); }
  return 0;
}


