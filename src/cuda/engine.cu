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
static DeviceJob* g_jobs = nullptr;
static unsigned int g_num_jobs = 0;

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

__global__ void kernel_mine_stub(unsigned int num_jobs, unsigned int nonce_base) {
  unsigned int j = blockIdx.y;
  if (j >= num_jobs) return;
  // Assemble minimal header fields from g_jobs; when SHA implemented, compute hash and compare targets
  if (threadIdx.x == 0 && blockIdx.x == 0 && g_jobs && g_hit_buf && g_hit_cap) {
    // Assemble 80-byte big-endian header from DeviceJob (scaffold)
    unsigned char header[80];
    // version
    header[0] = (g_jobs[j].version >> 24) & 0xFF; header[1] = (g_jobs[j].version >> 16) & 0xFF;
    header[2] = (g_jobs[j].version >> 8) & 0xFF;  header[3] = (g_jobs[j].version) & 0xFF;
    // prevhash (convert LE words to BE bytes)
    #pragma unroll
    for (int w = 0; w < 8; ++w) {
      unsigned int v = g_jobs[j].prevhash_le[w];
      header[4 + w*4 + 0] = (v >> 24) & 0xFF;
      header[4 + w*4 + 1] = (v >> 16) & 0xFF;
      header[4 + w*4 + 2] = (v >> 8) & 0xFF;
      header[4 + w*4 + 3] = (v) & 0xFF;
    }
    // merkle root
    #pragma unroll
    for (int w = 0; w < 8; ++w) {
      unsigned int v = g_jobs[j].merkle_root_le[w];
      header[36 + w*4 + 0] = (v >> 24) & 0xFF;
      header[36 + w*4 + 1] = (v >> 16) & 0xFF;
      header[36 + w*4 + 2] = (v >> 8) & 0xFF;
      header[36 + w*4 + 3] = (v) & 0xFF;
    }
    // ntime clamp within [ntime_min, ntime_max]
    unsigned int ntime = g_jobs[j].ntime;
    if (g_jobs[j].ntime_min && ntime < g_jobs[j].ntime_min) ntime = g_jobs[j].ntime_min;
    if (g_jobs[j].ntime_max && ntime > g_jobs[j].ntime_max) ntime = g_jobs[j].ntime_max;
    header[68] = (ntime >> 24) & 0xFF; header[69] = (ntime >> 16) & 0xFF;
    header[70] = (ntime >> 8) & 0xFF;  header[71] = (ntime) & 0xFF;
    // nbits
    unsigned int nbits = g_jobs[j].nbits;
    header[72] = (nbits >> 24) & 0xFF; header[73] = (nbits >> 16) & 0xFF;
    header[74] = (nbits >> 8) & 0xFF;  header[75] = (nbits) & 0xFF;
    // nonce
    unsigned int nonce = nonce_base + j;
    header[76] = (nonce >> 24) & 0xFF; header[77] = (nonce >> 16) & 0xFF;
    header[78] = (nonce >> 8) & 0xFF;  header[79] = (nonce) & 0xFF;

    // Compute SHA256d and compare against share_target (LE u32[8])
    unsigned char digest[32];
    bool used_midstate = false;
    // If any word of midstate is non-zero, assume a valid midstate was provided
    #pragma unroll
    for (int i=0;i<8;++i) { if (g_jobs[j].midstate_le[i] != 0u) { used_midstate = true; break; } }
    if (used_midstate) {
      // Compute SHA256(header) using provided midstate for first 64 bytes
      unsigned int st[8];
      #pragma unroll
      for (int i=0;i<8;++i) st[i] = g_jobs[j].midstate_le[i];
      // Prepare second block words (16 words, BE) for bytes 64..79 + padding + len=640 bits
      unsigned int w0_15[16];
      // bytes 64..79 are header[64..79]
      #pragma unroll
      for (int i=0;i<4;++i) {
        int o = 64 + i*4;
        w0_15[i] = (unsigned int(header[o])<<24) | (unsigned int(header[o+1])<<16) |
                   (unsigned int(header[o+2])<<8) | (unsigned int(header[o+3]));
      }
      w0_15[4] = 0x80000000u; // 0x80 then zeros
      #pragma unroll
      for (int i=5;i<15;++i) w0_15[i] = 0u;
      w0_15[15] = 640u;
      cuda_sha256d::sha256_compress(st, w0_15);
      // First digest (big-endian) now in st
      unsigned char t1[32];
      #pragma unroll
      for (int i=0;i<8;++i) {
        t1[i*4+0] = (unsigned char)((st[i] >> 24) & 0xFF);
        t1[i*4+1] = (unsigned char)((st[i] >> 16) & 0xFF);
        t1[i*4+2] = (unsigned char)((st[i] >> 8) & 0xFF);
        t1[i*4+3] = (unsigned char)((st[i]) & 0xFF);
      }
      // Second SHA over 32-byte digest
      unsigned int st2[8];
      #pragma unroll
      for (int i=0;i<8;++i) st2[i] = cuda_sha256d::kSha256IV[i]; // access constant via namespace if visible
      unsigned int w2[16];
      #pragma unroll
      for (int i=0;i<8;++i) {
        int o = i*4;
        w2[i] = (unsigned int(t1[o])<<24) | (unsigned int(t1[o+1])<<16) |
                (unsigned int(t1[o+2])<<8) | (unsigned int(t1[o+3]));
      }
      w2[8] = 0x80000000u; for (int i=9;i<15;++i) w2[i]=0u; w2[15] = 256u;
      cuda_sha256d::sha256_compress(st2, w2);
      // Output digest
      #pragma unroll
      for (int i=0;i<8;++i) {
        digest[i*4+0] = (unsigned char)((st2[i] >> 24) & 0xFF);
        digest[i*4+1] = (unsigned char)((st2[i] >> 16) & 0xFF);
        digest[i*4+2] = (unsigned char)((st2[i] >> 8) & 0xFF);
        digest[i*4+3] = (unsigned char)((st2[i]) & 0xFF);
      }
    } else {
      cuda_sha256d::sha256d_80_be(header, digest);
    }
    // Compare big-endian digest to precomputed big-endian share target
    bool leq = true;
    #pragma unroll
    for (int i=0;i<32;++i) {
      unsigned char t = g_jobs[j].share_target_be[i];
      if (digest[i] < t) { leq = true; break; }
      if (digest[i] > t) { leq = false; break; }
    }
    if (leq) {
      unsigned int idx = atomicInc(&g_hit_write_idx, 0xFFFFFFFFu);
      unsigned int slot = (g_hit_cap == 0) ? 0u : (idx % g_hit_cap);
      g_hit_buf[slot].work_id = g_jobs[j].work_id;
      g_hit_buf[slot].nonce = nonce;
    }
  }
}

__global__ void kernel_mine_batch(unsigned int num_jobs, unsigned int nonce_base, unsigned int nonces_per_thread) {
  unsigned int j = blockIdx.y;
  if (j >= num_jobs) return;
  unsigned int lane = threadIdx.x + blockIdx.x * blockDim.x;
  unsigned int start_nonce = nonce_base + j + lane * nonces_per_thread;
  // Iterate micro-batch per thread
  for (unsigned int k = 0; k < nonces_per_thread; ++k) {
    unsigned int nonce = start_nonce + k;
    if (threadIdx.x == 0 && blockIdx.x == 0) {
      // Reuse single-thread path for header assembly and hashing
      // Minimal duplication: call same body by inlining the key section
      unsigned char header[80];
      header[0] = (g_jobs[j].version >> 24) & 0xFF; header[1] = (g_jobs[j].version >> 16) & 0xFF;
      header[2] = (g_jobs[j].version >> 8) & 0xFF;  header[3] = (g_jobs[j].version) & 0xFF;
      #pragma unroll
      for (int w = 0; w < 8; ++w) {
        unsigned int v = g_jobs[j].prevhash_le[w];
        header[4 + w*4 + 0] = (v >> 24) & 0xFF;
        header[4 + w*4 + 1] = (v >> 16) & 0xFF;
        header[4 + w*4 + 2] = (v >> 8) & 0xFF;
        header[4 + w*4 + 3] = (v) & 0xFF;
      }
      #pragma unroll
      for (int w = 0; w < 8; ++w) {
        unsigned int v = g_jobs[j].merkle_root_le[w];
        header[36 + w*4 + 0] = (v >> 24) & 0xFF;
        header[36 + w*4 + 1] = (v >> 16) & 0xFF;
        header[36 + w*4 + 2] = (v >> 8) & 0xFF;
        header[36 + w*4 + 3] = (v) & 0xFF;
      }
      unsigned int ntime = g_jobs[j].ntime;
      if (g_jobs[j].ntime_min && ntime < g_jobs[j].ntime_min) ntime = g_jobs[j].ntime_min;
      if (g_jobs[j].ntime_max && ntime > g_jobs[j].ntime_max) ntime = g_jobs[j].ntime_max;
      header[68] = (ntime >> 24) & 0xFF; header[69] = (ntime >> 16) & 0xFF;
      header[70] = (ntime >> 8) & 0xFF;  header[71] = (ntime) & 0xFF;
      unsigned int nbits = g_jobs[j].nbits;
      header[72] = (nbits >> 24) & 0xFF; header[73] = (nbits >> 16) & 0xFF;
      header[74] = (nbits >> 8) & 0xFF;  header[75] = (nbits) & 0xFF;
      header[76] = (nonce >> 24) & 0xFF; header[77] = (nonce >> 16) & 0xFF;
      header[78] = (nonce >> 8) & 0xFF;  header[79] = (nonce) & 0xFF;
      unsigned char digest[32];
      cuda_sha256d::sha256d_80_be(header, digest);
      bool leq = true;
      #pragma unroll
      for (int i=0;i<32;++i) {
        unsigned char t = g_jobs[j].share_target_be[i];
        if (digest[i] < t) { leq = true; break; }
        if (digest[i] > t) { leq = false; break; }
      }
      if (leq) {
        unsigned int idx = atomicInc(&g_hit_write_idx, 0xFFFFFFFFu);
        unsigned int slot = (g_hit_cap == 0) ? 0u : (idx % g_hit_cap);
        g_hit_buf[slot].work_id = g_jobs[j].work_id;
        g_hit_buf[slot].nonce = nonce;
      }
    }
  }
}
__global__ void kernel_hash_one(uint32_t job_index, uint32_t nonce, unsigned char* out32) {
  if (!g_jobs) return;
  unsigned int j = job_index;
  unsigned char header[80];
  // version
  header[0] = (g_jobs[j].version >> 24) & 0xFF; header[1] = (g_jobs[j].version >> 16) & 0xFF;
  header[2] = (g_jobs[j].version >> 8) & 0xFF;  header[3] = (g_jobs[j].version) & 0xFF;
  // prevhash
  #pragma unroll
  for (int w = 0; w < 8; ++w) {
    unsigned int v = g_jobs[j].prevhash_le[w];
    header[4 + w*4 + 0] = (v >> 24) & 0xFF;
    header[4 + w*4 + 1] = (v >> 16) & 0xFF;
    header[4 + w*4 + 2] = (v >> 8) & 0xFF;
    header[4 + w*4 + 3] = (v) & 0xFF;
  }
  // merkle
  #pragma unroll
  for (int w = 0; w < 8; ++w) {
    unsigned int v = g_jobs[j].merkle_root_le[w];
    header[36 + w*4 + 0] = (v >> 24) & 0xFF;
    header[36 + w*4 + 1] = (v >> 16) & 0xFF;
    header[36 + w*4 + 2] = (v >> 8) & 0xFF;
    header[36 + w*4 + 3] = (v) & 0xFF;
  }
  // ntime
  unsigned int ntime = g_jobs[j].ntime;
  if (g_jobs[j].ntime_min && ntime < g_jobs[j].ntime_min) ntime = g_jobs[j].ntime_min;
  if (g_jobs[j].ntime_max && ntime > g_jobs[j].ntime_max) ntime = g_jobs[j].ntime_max;
  header[68] = (ntime >> 24) & 0xFF; header[69] = (ntime >> 16) & 0xFF;
  header[70] = (ntime >> 8) & 0xFF;  header[71] = (ntime) & 0xFF;
  // nbits
  unsigned int nbits = g_jobs[j].nbits;
  header[72] = (nbits >> 24) & 0xFF; header[73] = (nbits >> 16) & 0xFF;
  header[74] = (nbits >> 8) & 0xFF;  header[75] = (nbits) & 0xFF;
  // nonce
  header[76] = (nonce >> 24) & 0xFF; header[77] = (nonce >> 16) & 0xFF;
  header[78] = (nonce >> 8) & 0xFF;  header[79] = (nonce) & 0xFF;
  unsigned char digest[32];
  cuda_sha256d::sha256d_80_be(header, digest);
  // write digest to out32
  for (int i=0;i<32;++i) out32[i] = digest[i];
}

bool cuda_engine::computeDeviceHashForJob(uint32_t job_index, uint32_t nonce, unsigned char out32_host[32]) {
#ifndef __CUDACC__
  (void)job_index; (void)nonce; (void)out32_host; return true;
#else
  if (!g_jobs) return false;
  unsigned char* d_out = nullptr;
  cudaMalloc(&d_out, 32);
  kernel_hash_one<<<1,1>>>(job_index, nonce, d_out);
  cudaDeviceSynchronize();
  cudaMemcpy(out32_host, d_out, 32, cudaMemcpyDeviceToHost);
  cudaFree(d_out);
  return true;
#endif
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

bool cuda_engine::launchMineStub(uint32_t num_jobs, uint32_t nonce_base) {
#ifndef __CUDACC__
  (void)num_jobs; (void)nonce_base; return true;
#else
  if (g_num_jobs == 0 || num_jobs == 0) return false;
  dim3 grid(1, num_jobs, 1);
  dim3 block(64, 1, 1);
  kernel_mine_stub<<<grid, block>>>(num_jobs, nonce_base);
  cudaDeviceSynchronize();
  return true;
#endif
}

bool cuda_engine::launchMineWithPlan(uint32_t num_jobs,
                                     uint32_t blocks_per_job,
                                     uint32_t threads_per_block,
                                     uint32_t nonce_base) {
#ifndef __CUDACC__
  (void)num_jobs; (void)blocks_per_job; (void)threads_per_block; (void)nonce_base; return true;
#else
  if (g_num_jobs == 0 || num_jobs == 0) return false;
  if (blocks_per_job == 0 || threads_per_block == 0) return false;
  dim3 grid(blocks_per_job, num_jobs, 1);
  dim3 block(threads_per_block, 1, 1);
  kernel_mine_stub<<<grid, block>>>(num_jobs, nonce_base);
  cudaDeviceSynchronize();
  return true;
#endif
}

bool cuda_engine::launchMineWithPlanBatch(uint32_t num_jobs,
                                          uint32_t blocks_per_job,
                                          uint32_t threads_per_block,
                                          uint32_t nonce_base,
                                          uint32_t nonces_per_thread) {
#ifndef __CUDACC__
  (void)num_jobs; (void)blocks_per_job; (void)threads_per_block; (void)nonce_base; (void)nonces_per_thread; return true;
#else
  if (g_num_jobs == 0 || num_jobs == 0) return false;
  if (blocks_per_job == 0 || threads_per_block == 0) return false;
  if (nonces_per_thread == 0) nonces_per_thread = 1;
  dim3 grid(blocks_per_job, num_jobs, 1);
  dim3 block(threads_per_block, 1, 1);
  kernel_mine_batch<<<grid, block>>>(num_jobs, nonce_base, nonces_per_thread);
  cudaDeviceSynchronize();
  return true;
#endif
}

bool cuda_engine::uploadDeviceJobs(const DeviceJob* jobs_host, uint32_t num_jobs) {
#ifndef __CUDACC__
  (void)jobs_host; (void)num_jobs; return true;
#else
  if (num_jobs == 0) return false;
  if (g_jobs) cudaFree(g_jobs);
  cudaMalloc(&g_jobs, sizeof(DeviceJob) * num_jobs);
  cudaMemcpy(g_jobs, jobs_host, sizeof(DeviceJob) * num_jobs, cudaMemcpyHostToDevice);
  g_num_jobs = num_jobs;
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


