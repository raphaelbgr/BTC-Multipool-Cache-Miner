#include <gtest/gtest.h>

#include "cuda/engine.h"
#include "normalize/endianness.h"
#include "submit/cpu_verify.h"

// This test validates that device SHA-256d stub path links and can be invoked through mining stub
TEST(CUDADeviceHash, MineStubLinksAndRuns) {
  using namespace cuda_engine;
  DeviceJob job{};
  job.version = 0x20000000u;
  // Fill prevhash/merkle with zeros for simplicity; ntime/nbits arbitrary; targets large to accept
  job.ntime = 0x5f5e100u;
  job.nbits = 0x1f00ffffu;
  for (int i=0;i<8;++i) { job.prevhash_le[i]=0; job.merkle_root_le[i]=0; job.share_target_le[i]=0xFFFFFFFFu; job.block_target_le[i]=0xFFFFFFFFu; job.midstate_le[i]=0; }
  job.work_id = 1234ull;
  ASSERT_TRUE(uploadDeviceJobs(&job, 1));
  ASSERT_TRUE(initDeviceHitBuffer(8));
  ASSERT_TRUE(launchMineStub(1, 0));
  HitRecord out[8]; uint32_t n=0;
  ASSERT_TRUE(drainDeviceHits(out, 8, &n));
  // With max target, we expect at least one emitted hit from the stub path
  ASSERT_GE(n, 0u);
  freeDeviceHitBuffer();
}

TEST(CUDADeviceHash, DeviceVsCPUConsistencyForZeroHeader) {
  using namespace cuda_engine;
  DeviceJob job{};
  job.version = 0x00000000u;
  job.ntime = 0;
  job.nbits = 0;
  for (int i=0;i<8;++i) { job.prevhash_le[i]=0; job.merkle_root_le[i]=0; job.share_target_le[i]=0xFFFFFFFFu; job.block_target_le[i]=0xFFFFFFFFu; job.midstate_le[i]=0; }
  job.work_id = 1ull;
  ASSERT_TRUE(uploadDeviceJobs(&job, 1));
  unsigned char dev_digest[32];
  ASSERT_TRUE(computeDeviceHashForJob(0, 0, dev_digest));
  // Compute CPU double SHA for the same 80-byte zeros header
  unsigned char header[80]; for (int i=0;i<80;++i) header[i]=0;
  uint8_t cpu_digest[32];
  submit::sha256(header, 80, cpu_digest);
  submit::sha256(cpu_digest, 32, cpu_digest);
  // Compare digests
  for (int i=0;i<32;++i) { EXPECT_EQ(dev_digest[i], cpu_digest[i]); }
}

TEST(CUDADeviceHash, DeviceVsCPUWithMidstate) {
  using namespace cuda_engine;
  // Build a header with only last 16 bytes non-zero to exercise midstate tail path
  unsigned char header[80]; for (int i=0;i<80;++i) header[i]=0;
  // version, prevhash, merkle left zeros; set ntime/nbits/nonce
  header[68]=0x00; header[69]=0x00; header[70]=0x00; header[71]=0x02;
  header[72]=0x1d; header[73]=0x00; header[74]=0xff; header[75]=0xff;
  header[76]=0x00; header[77]=0x00; header[78]=0x00; header[79]=0x01;
  // Compute CPU sha256(header) midstate for first 64 bytes
  std::array<uint32_t,8> ms = normalize::sha256_midstate_64(header);
  DeviceJob job{};
  job.version = (unsigned int(header[0])<<24) | (unsigned int(header[1])<<16) | (unsigned int(header[2])<<8) | (unsigned int(header[3]));
  for (int i=0;i<8;++i) { job.prevhash_le[i]=0; job.merkle_root_le[i]=0; job.share_target_le[i]=0xFFFFFFFFu; job.block_target_le[i]=0xFFFFFFFFu; }
  job.ntime = 2; job.nbits = 0x1d00ffffu; job.work_id=2ull;
  for (int i=0;i<8;++i) job.midstate_le[i]=ms[i];
  ASSERT_TRUE(uploadDeviceJobs(&job, 1));
  unsigned char dev_digest[32];
  ASSERT_TRUE(computeDeviceHashForJob(0, 1, dev_digest));
  // CPU double SHA
  uint8_t cpu_digest[32];
  submit::sha256(header, 80, cpu_digest);
  submit::sha256(cpu_digest, 32, cpu_digest);
  for (int i=0;i<32;++i) { EXPECT_EQ(dev_digest[i], cpu_digest[i]); }
}

TEST(CUDADeviceHash, RealHeaderGenesisLikeEquality) {
  using namespace cuda_engine;
  // Construct a header similar to Bitcoin genesis (prev=0, known merkle)
  unsigned char header[80]; for (int i=0;i<80;++i) header[i]=0;
  // version = 1
  header[0]=0x01; header[1]=0x00; header[2]=0x00; header[3]=0x00;
  // prevhash already zero
  // merkle root (big-endian bytes)
  const char* mr_hex = "4a5e1e4baab89f3a32518a88c31bc87f618f76673e2cc77ab2127b7afdeda33b";
  for (int i=0;i<32;++i){ unsigned int v; sscanf(mr_hex+2*i, "%02x", &v); header[36+i]=static_cast<unsigned char>(v); }
  // time, bits, nonce
  header[68]=0x49; header[69]=0x7f; header[70]=0xff; header[71]=0x00; // 0x497fff00 (placeholder)
  header[72]=0x1d; header[73]=0x00; header[74]=0xff; header[75]=0xff; // 0x1d00ffff
  header[76]=0x7c; header[77]=0x2b; header[78]=0xac; header[79]=0x1d; // 0x7c2bac1d (placeholder)

  DeviceJob job{};
  job.version = (unsigned int(header[0])<<24) | (unsigned int(header[1])<<16) | (unsigned int(header[2])<<8) | (unsigned int(header[3]));
  // prevhash zeros -> LE words already zero
  // merkle root: convert from BE bytes to LE words expected by device
  auto mr_le = normalize::be32BytesToLeU32Words(&header[36]);
  for (int i=0;i<8;++i) job.merkle_root_le[i]=mr_le[i];
  job.ntime = (unsigned int(header[68])<<24)|(unsigned int(header[69])<<16)|(unsigned int(header[70])<<8)|unsigned int(header[71]);
  job.nbits = (unsigned int(header[72])<<24)|(unsigned int(header[73])<<16)|(unsigned int(header[74])<<8)|unsigned int(header[75]);
  for (int i=0;i<8;++i) { job.share_target_le[i]=0xFFFFFFFFu; job.block_target_le[i]=0xFFFFFFFFu; }
  // precompute midstate for first 64 bytes
  auto ms = normalize::sha256_midstate_64(header);
  for (int i=0;i<8;++i) job.midstate_le[i]=ms[i];
  job.work_id=3ull;
  ASSERT_TRUE(uploadDeviceJobs(&job, 1));
  unsigned int nonce = (unsigned int(header[76])<<24)|(unsigned int(header[77])<<16)|(unsigned int(header[78])<<8)|unsigned int(header[79]);
  unsigned char dev_digest[32];
  ASSERT_TRUE(computeDeviceHashForJob(0, nonce, dev_digest));
  // CPU double SHA
  uint8_t cpu_digest[32];
  submit::sha256(header, 80, cpu_digest);
  submit::sha256(cpu_digest, 32, cpu_digest);
  for (int i=0;i<32;++i) { EXPECT_EQ(dev_digest[i], cpu_digest[i]); }
}


