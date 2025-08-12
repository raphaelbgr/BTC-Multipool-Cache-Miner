#include "cuda/engine.h"
#include "cuda/sha256d.cuh"
#include "cuda/launch_plan.h"
#include <vector>

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

// Simple global buffers for a device-side ring
static __device__ unsigned int g_hit_write_idx = 0;
static HitRecordDevice* g_hit_buf = nullptr;
static unsigned int g_hit_cap = 0;

__global__ void kernel_init_hit_buf(HitRecordDevice* buf, unsigned int cap) {
  g_hit_buf = buf;
  g_hit_cap = cap;
  g_hit_write_idx = 0;
}

__global__ void kernel_push_hits(const unsigned long long* work_ids,
                                 unsigned int num_jobs,
                                 unsigned int nonce_base) {
  unsigned int j = blockIdx.y;
  if (j >= num_jobs) return;
  if (threadIdx.x == 0 && blockIdx.x == 0) {
    unsigned int idx = atomicInc(&g_hit_write_idx, 0xFFFFFFFFu);
    unsigned int slot = (g_hit_cap == 0) ? 0u : (idx % g_hit_cap);
    if (g_hit_buf && g_hit_cap) {
      g_hit_buf[slot].work_id = work_ids[j];
      g_hit_buf[slot].nonce = nonce_base + j;
    }
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

static HitRecordDevice* s_device_hit_buf = nullptr;
static unsigned int s_device_hit_cap = 0;
static unsigned int s_device_drain_offset = 0; // host-side read position (monotonic)

bool cuda_engine::initDeviceHitBuffer(uint32_t capacity) {
#ifndef __CUDACC__
  (void)capacity; return true;
#else
  if (capacity == 0) capacity = 1;
  if (s_device_hit_buf) cudaFree(s_device_hit_buf);
  s_device_hit_cap = capacity;
  cudaMalloc(&s_device_hit_buf, sizeof(HitRecordDevice) * capacity);
  kernel_init_hit_buf<<<1,1>>>(s_device_hit_buf, capacity);
  cudaDeviceSynchronize();
  s_device_drain_offset = 0;
  return true;
#endif
}

void cuda_engine::freeDeviceHitBuffer() {
#ifdef __CUDACC__
  if (s_device_hit_buf) cudaFree(s_device_hit_buf);
  s_device_hit_buf = nullptr;
  s_device_hit_cap = 0;
#endif
}

bool cuda_engine::launchPushHitsToDeviceRing(const uint64_t* work_ids_host,
                                             uint32_t num_jobs,
                                             uint32_t nonce_base) {
#ifndef __CUDACC__
  (void)work_ids_host; (void)num_jobs; (void)nonce_base; return true;
#else
  if (!s_device_hit_buf || s_device_hit_cap == 0) return false;
  if (num_jobs == 0) return true;
  unsigned long long* d_work_ids = nullptr;
  cudaMalloc(&d_work_ids, sizeof(unsigned long long) * num_jobs);
  cudaMemcpy(d_work_ids, work_ids_host, sizeof(unsigned long long) * num_jobs, cudaMemcpyHostToDevice);
  dim3 grid(1, num_jobs, 1);
  dim3 block(64, 1, 1);
  kernel_push_hits<<<grid, block>>>(d_work_ids, num_jobs, nonce_base);
  cudaDeviceSynchronize();
  cudaFree(d_work_ids);
  return true;
#endif
}

bool cuda_engine::drainDeviceHits(HitRecord* out_hits_host,
                                  uint32_t max_out,
                                  uint32_t* out_count) {
#ifndef __CUDACC__
  *out_count = 0; return true;
#else
  if (!s_device_hit_buf || s_device_hit_cap == 0) { *out_count = 0; return false; }
  // Read current write idx from device symbol
  unsigned int write_idx = 0;
  cudaMemcpyFromSymbol(&write_idx, g_hit_write_idx, sizeof(unsigned int));
  // Determine how many entries were written since last drain (modulo 2^32)
  unsigned int available = (write_idx >= s_device_drain_offset)
                             ? (write_idx - s_device_drain_offset)
                             : (0xFFFFFFFFu - s_device_drain_offset + 1u + write_idx);
  if (available == 0) { *out_count = 0; return true; }
  unsigned int to_copy = (available > max_out) ? max_out : available;
  // Copy from ring with wrap handling
  unsigned int start = s_device_drain_offset % s_device_hit_cap;
  unsigned int first = (to_copy < (s_device_hit_cap - start)) ? to_copy : (s_device_hit_cap - start);
  std::vector<HitRecordDevice> tmp(to_copy);
  if (first > 0) {
    cudaMemcpy(tmp.data(), s_device_hit_buf + start, sizeof(HitRecordDevice) * first, cudaMemcpyDeviceToHost);
  }
  if (first < to_copy) {
    cudaMemcpy(tmp.data() + first, s_device_hit_buf, sizeof(HitRecordDevice) * (to_copy - first), cudaMemcpyDeviceToHost);
  }
  for (unsigned int i = 0; i < to_copy; ++i) {
    out_hits_host[i].work_id = static_cast<uint64_t>(tmp[i].work_id);
    out_hits_host[i].nonce = static_cast<uint32_t>(tmp[i].nonce);
  }
  *out_count = to_copy;
  s_device_drain_offset += to_copy;
  return true;
#endif
}

}  // namespace cuda_engine


