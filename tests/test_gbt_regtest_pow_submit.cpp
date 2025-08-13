#include <gtest/gtest.h>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <vector>
#include <sstream>

#include <nlohmann/json.hpp>

#include "net/rpc_client.h"
#include "submit/gbt_submitter.h"
#include "submit/cpu_verify.h"
#include "normalize/targets.h"

static std::string getenv_str(const char* k) {
  const char* v = std::getenv(k);
  return v ? std::string(v) : std::string();
}

static std::vector<uint8_t> hexToBytes(const std::string& hex) {
  std::vector<uint8_t> out;
  out.resize(hex.size() / 2);
  for (size_t i = 0; i + 1 < hex.size(); i += 2) {
    unsigned int v; sscanf(hex.c_str() + i, "%02x", &v); out[i/2] = static_cast<uint8_t>(v);
  }
  return out;
}

static void store_u32_be(uint8_t* p, uint32_t v) {
  p[0] = static_cast<uint8_t>((v >> 24) & 0xFF);
  p[1] = static_cast<uint8_t>((v >> 16) & 0xFF);
  p[2] = static_cast<uint8_t>((v >> 8) & 0xFF);
  p[3] = static_cast<uint8_t>(v & 0xFF);
}

TEST(GBTRegtest, POWSubmitblockAcceptance_EnvGated) {
  std::string url = getenv_str("BMAD_REGTEST_RPC_URL");
  if (url.empty()) GTEST_SKIP() << "BMAD_REGTEST_RPC_URL not set";
  int max_iters = 2000000; // default 2M
  if (const char* mi = std::getenv("BMAD_REGTEST_MAX_ITERS")) {
    max_iters = std::max(1000, atoi(mi));
  }
  net::RpcClient rpc(url);
  std::string cookie = getenv_str("BMAD_REGTEST_COOKIE");
  if (!cookie.empty()) rpc.setCookieAuthFile(cookie);

  // Fetch template
  nlohmann::json params = nlohmann::json::array();
  nlohmann::json p = nlohmann::json::object();
  p["rules"] = nlohmann::json::array({"segwit"});
  params.push_back(p);
  auto tmpl = rpc.call("getblocktemplate", params);
  ASSERT_TRUE(tmpl.has_value()) << rpc.lastError();
  const auto& T = *tmpl;
  // Require coinbasetxn for real submit
  if (!T.contains("coinbasetxn")) GTEST_SKIP() << "coinbasetxn not provided by node";

  // Prepare submitter
  config::PoolEntry::RpcConfig rc; rc.url = url; rc.auth = "cookie";
  submit::GbtSubmitter sub(rc);
  sub.updateTemplate(T);
  ASSERT_TRUE(sub.hasCoinbase());

  // Build header fields
  uint32_t version = static_cast<uint32_t>(T["version"].get<int>());
  std::string prevhash_hex = T["previousblockhash"].get<std::string>();
  std::string merkleroot_hex = T["merkleroot"].get<std::string>();
  uint32_t ntime = static_cast<uint32_t>(T["curtime"].get<int64_t>());
  uint32_t nbits = std::stoul(T["bits"].get<std::string>(), nullptr, 16);

  // Header 80 bytes (big-endian fields)
  uint8_t header[80];
  store_u32_be(&header[0], version);
  auto prev = hexToBytes(prevhash_hex); ASSERT_EQ(prev.size(), 32u);
  auto mr = hexToBytes(merkleroot_hex); ASSERT_EQ(mr.size(), 32u);
  memcpy(&header[4], prev.data(), 32);
  memcpy(&header[36], mr.data(), 32);
  store_u32_be(&header[68], ntime);
  store_u32_be(&header[72], nbits);

  // Target from compact
  auto target_le = normalize::compactToTargetU32LE(nbits);

  // Brute-force nonce
  bool found = false; uint32_t found_nonce = 0;
  for (int i = 0; i < max_iters; ++i) {
    uint32_t nonce = static_cast<uint32_t>(i);
    store_u32_be(&header[76], nonce);
    uint8_t h1[32]; submit::sha256(header, 80, h1);
    uint8_t h2[32]; submit::sha256(h1, 32, h2);
    if (submit::isHashLEQTarget(h2, target_le)) { found = true; found_nonce = nonce; break; }
  }
  if (!found) GTEST_SKIP() << "did not find POW within BMAD_REGTEST_MAX_ITERS";

  // Submit using submitter
  bool ok = sub.submitHeader(header);
  EXPECT_TRUE(ok);
}




