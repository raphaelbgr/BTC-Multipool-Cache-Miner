#include <chrono>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <vector>

#include "cuda/engine.h"
#include "cuda/launch_plan.h"

int main(int argc, char** argv) {
  uint32_t jobs = 8;
  uint32_t desired_threads = 256;
  uint32_t nonces_per_thread = 1;
  if (argc > 1) desired_threads = static_cast<uint32_t>(std::max(1, atoi(argv[1])));
  if (argc > 2) nonces_per_thread = static_cast<uint32_t>(std::max(1, atoi(argv[2])));

  // Prepare dummy jobs
  std::vector<cuda_engine::DeviceJob> dj(jobs);
  for (uint32_t j = 0; j < jobs; ++j) {
    dj[j].version = 0x20000000u;
    dj[j].ntime = 0x5f5e100u;
    dj[j].nbits = 0x207fffffu; // very easy target
    dj[j].work_id = j + 1;
  }
  cuda_engine::uploadDeviceJobs(dj.data(), jobs);

  auto plan = cuda_engine::computeLaunchPlan(jobs, desired_threads);
  if (plan.num_jobs == 0) {
    std::cerr << "bad plan" << std::endl; return 1;
  }
  auto t0 = std::chrono::steady_clock::now();
  if (nonces_per_thread > 1) {
    cuda_engine::launchMineWithPlanBatch(plan.num_jobs, plan.blocks_per_job, plan.threads_per_block, 0, nonces_per_thread);
  } else {
    cuda_engine::launchMineWithPlan(plan.num_jobs, plan.blocks_per_job, plan.threads_per_block, 0);
  }
  auto t1 = std::chrono::steady_clock::now();
  double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
  double tested = static_cast<double>(plan.blocks_per_job) * plan.threads_per_block * jobs * std::max(1u, nonces_per_thread);
  double khs = (tested / (ms / 1000.0)) / 1000.0;
  float occ = 0.0f; int sms=0, ab=0, maxthr=0; cuda_engine::getMineBatchOccupancy(plan.threads_per_block, &occ, &sms, &ab, &maxthr);
  std::cout << std::fixed << std::setprecision(2)
            << "threads_per_block=" << plan.threads_per_block
            << " blocks_per_job=" << plan.blocks_per_job
            << " jobs=" << plan.num_jobs
            << " nonces_per_thread=" << nonces_per_thread
            << " ms=" << ms
            << " khs=" << khs
            << " occ_pct=" << (occ * 100.0f)
            << std::endl;
  return 0;
}



