#pragma once

#ifdef __CUDACC__
#define MCBCM_DEVICE __device__
#define MCBCM_GLOBAL __global__
#else
#define MCBCM_DEVICE
#define MCBCM_GLOBAL
#endif

namespace cuda_sha256d {

// SHA-256 constants
#ifdef __CUDACC__
static __device__ __constant__ unsigned int kSha256K[64] = {
  0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
  0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
  0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
  0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
  0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
  0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
  0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
  0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
};
static __device__ __constant__ unsigned int kSha256IV[8] = {
  0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19
};

MCBCM_DEVICE inline unsigned int rotr(unsigned int x, int n) { return (x >> n) | (x << (32 - n)); }
MCBCM_DEVICE inline unsigned int Ch(unsigned int x, unsigned int y, unsigned int z) { return (x & y) ^ (~x & z); }
MCBCM_DEVICE inline unsigned int Maj(unsigned int x, unsigned int y, unsigned int z) { return (x & y) ^ (x & z) ^ (y & z); }
MCBCM_DEVICE inline unsigned int Sig0(unsigned int x) { return rotr(x,2) ^ rotr(x,13) ^ rotr(x,22); }
MCBCM_DEVICE inline unsigned int Sig1(unsigned int x) { return rotr(x,6) ^ rotr(x,11) ^ rotr(x,25); }
MCBCM_DEVICE inline unsigned int sig0(unsigned int x) { return rotr(x,7) ^ rotr(x,18) ^ (x >> 3); }
MCBCM_DEVICE inline unsigned int sig1(unsigned int x) { return rotr(x,17) ^ rotr(x,19) ^ (x >> 10); }

MCBCM_DEVICE inline void sha256_compress(unsigned int state[8], const unsigned int w0_15_be[16]) {
  unsigned int w[64];
  #pragma unroll
  for (int i = 0; i < 16; ++i) {
    // big-endian input
    unsigned int v = w0_15_be[i];
    w[i] = v;
  }
  #pragma unroll
  for (int i = 16; i < 64; ++i) {
    w[i] = sig1(w[i-2]) + w[i-7] + sig0(w[i-15]) + w[i-16];
  }
  unsigned int a = state[0], b = state[1], c = state[2], d = state[3];
  unsigned int e = state[4], f = state[5], g = state[6], h = state[7];
  #pragma unroll
  for (int i = 0; i < 64; ++i) {
    unsigned int t1 = h + Sig1(e) + Ch(e,f,g) + kSha256K[i] + w[i];
    unsigned int t2 = Sig0(a) + Maj(a,b,c);
    h = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
  }
  state[0] += a; state[1] += b; state[2] += c; state[3] += d;
  state[4] += e; state[5] += f; state[6] += g; state[7] += h;
}

// Compute SHA256(header80) and store big-endian 32-byte digest to out32
MCBCM_DEVICE inline void sha256_80_be(const unsigned char header80[80], unsigned char out32[32]) {
  // First block: header[0..63]
  unsigned int st[8];
  #pragma unroll
  for (int i=0;i<8;++i) st[i] = kSha256IV[i];
  unsigned int w0_15[16];
  #pragma unroll
  for (int i=0;i<16;++i) {
    int o = i*4;
    w0_15[i] = (unsigned int(header80[o])<<24) | (unsigned int(header80[o+1])<<16) |
               (unsigned int(header80[o+2])<<8) | (unsigned int(header80[o+3]));
  }
  sha256_compress(st, w0_15);
  // Second block: header[64..79] + 0x80 + zero pad + len=640 bits
  unsigned char block[64];
  #pragma unroll
  for (int i=0;i<16;++i) block[i] = header80[64+i];
  block[16] = 0x80; // 0x80 padding
  #pragma unroll
  for (int i=17;i<56;++i) block[i] = 0;
  unsigned long long bitlen = 640ull;
  // write big-endian 64-bit length into last 8 bytes
  for (int i=0;i<8;++i) block[56+i] = (unsigned char)((bitlen >> (56 - 8*i)) & 0xFF);
  // Prepare words and compress
  #pragma unroll
  for (int i=0;i<16;++i) {
    int o = i*4;
    w0_15[i] = (unsigned int(block[o])<<24) | (unsigned int(block[o+1])<<16) |
               (unsigned int(block[o+2])<<8) | (unsigned int(block[o+3]));
  }
  sha256_compress(st, w0_15);
  // Produce big-endian digest
  #pragma unroll
  for (int i=0;i<8;++i) {
    out32[i*4+0] = (unsigned char)((st[i] >> 24) & 0xFF);
    out32[i*4+1] = (unsigned char)((st[i] >> 16) & 0xFF);
    out32[i*4+2] = (unsigned char)((st[i] >> 8) & 0xFF);
    out32[i*4+3] = (unsigned char)((st[i]) & 0xFF);
  }
}

// Double SHA256: SHA256(SHA256(header80)) -> out32 (big-endian)
MCBCM_DEVICE inline void sha256d_80_be(const unsigned char header80[80], unsigned char out32[32]) {
  unsigned char t1[32];
  sha256_80_be(header80, t1);
  // Hash 32-byte digest: single block message
  unsigned int st[8];
  #pragma unroll
  for (int i=0;i<8;++i) st[i] = kSha256IV[i];
  unsigned int w0_15[16];
  // 32 bytes data, then 0x80, zeros, len=256 bits
  #pragma unroll
  for (int i=0;i<8;++i) {
    int o = i*4;
    w0_15[i] = (unsigned int(t1[o])<<24) | (unsigned int(t1[o+1])<<16) |
               (unsigned int(t1[o+2])<<8) | (unsigned int(t1[o+3]));
  }
  w0_15[8] = 0x80000000u;
  #pragma unroll
  for (int i=9;i<15;++i) w0_15[i] = 0u;
  w0_15[15] = 256u; // bit length
  sha256_compress(st, w0_15);
  // output big-endian
  #pragma unroll
  for (int i=0;i<8;++i) {
    out32[i*4+0] = (unsigned char)((st[i] >> 24) & 0xFF);
    out32[i*4+1] = (unsigned char)((st[i] >> 16) & 0xFF);
    out32[i*4+2] = (unsigned char)((st[i] >> 8) & 0xFF);
    out32[i*4+3] = (unsigned char)((st[i]) & 0xFF);
  }
}

MCBCM_DEVICE inline void hash256_once_stub() {}
#endif

}


