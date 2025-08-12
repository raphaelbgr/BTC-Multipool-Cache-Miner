#include "cuda/engine.h"
#include "cuda/sha256d.cuh"
#include "cuda/launch_plan.h"

#include <cuda_runtime.h>

namespace cuda_engine {

__global__ void kernel_noop() {
  cuda_sha256d::hash256_once_stub();
}

__global__ void kernel_multi_noop() {
  // y-dimension indexes job, x-dimension covers blocks; no-op body for now
  cuda_sha256d::hash256_once_stub();
}

struct HitRecordDevice { unsigned long long work_id; unsigned int nonce; };

__global__ void kernel_write_hits(const unsigned long long* work_ids,
                                  unsigned int num_jobs,
                                  unsigned int nonce_base,
                                  HitRecordDevice* out_hits) {
  unsigned int j = blockIdx.y;
  if (j >= num_jobs) return;
  if (threadIdx.x == 0 && blockIdx.x == 0) {
    out_hits[j].work_id = work_ids[j];
    out_hits[j].nonce = nonce_base + j;
  }
}

void launchStub(const LaunchParams& params) {
  if (params.blocks == 0 || params.threads_per_block == 0) return;
  kernel_noop<<<params.blocks, params.threads_per_block>>>();
  cudaDeviceSynchronize();
}

bool cuda_engine::launchMultiJobStub(uint32_t num_jobs, uint64_t desired_threads_per_job) {
#ifndef __CUDACC__
  (void)num_jobs; (void)desired_threads_per_job;
  return false;
#else
  auto plan = computeLaunchPlan(num_jobs, desired_threads_per_job);
  if (plan.num_jobs == 0 || plan.blocks_per_job == 0 || plan.threads_per_block == 0) return false;
  dim3 grid(plan.blocks_per_job, plan.num_jobs, 1);
  dim3 block(plan.threads_per_block, 1, 1);
  kernel_multi_noop<<<grid, block>>>();
  cudaDeviceSynchronize();
  return true;
#endif
}

bool cuda_engine::launchWriteHitsDemo(const uint64_t* work_ids_host,
                                      uint32_t num_jobs,
                                      uint32_t nonce_base,
                                      HitRecord* out_hits_host) {
#ifndef __CUDACC__
  // Fallback: fill on host for non-CUDA builds
  for (uint32_t j = 0; j < num_jobs; ++j) {
    out_hits_host[j].work_id = work_ids_host[j];
    out_hits_host[j].nonce = nonce_base + j;
  }
  return true;
#else
  if (num_jobs == 0) return false;
  unsigned long long* d_work_ids = nullptr;
  HitRecordDevice* d_hits = nullptr;
  cudaMalloc(&d_work_ids, sizeof(unsigned long long) * num_jobs);
  cudaMalloc(&d_hits, sizeof(HitRecordDevice) * num_jobs);
  cudaMemcpy(d_work_ids, work_ids_host, sizeof(unsigned long long) * num_jobs, cudaMemcpyHostToDevice);
  dim3 grid(1, num_jobs, 1);
  dim3 block(64, 1, 1);
  kernel_write_hits<<<grid, block>>>(d_work_ids, num_jobs, nonce_base, d_hits);
  cudaDeviceSynchronize();
  // Copy back
  std::vector<HitRecordDevice> tmp(num_jobs);
  cudaMemcpy(tmp.data(), d_hits, sizeof(HitRecordDevice) * num_jobs, cudaMemcpyDeviceToHost);
  for (uint32_t j = 0; j < num_jobs; ++j) {
    out_hits_host[j].work_id = static_cast<uint64_t>(tmp[j].work_id);
    out_hits_host[j].nonce = static_cast<uint32_t>(tmp[j].nonce);
  }
  cudaFree(d_hits);
  cudaFree(d_work_ids);
  return true;
#endif
}

}  // namespace cuda_engine


