#include <gtest/gtest.h>
#include <cstdlib>
#include <string>
#include <sstream>
#include <nlohmann/json.hpp>

#include "net/rpc_client.h"
#include "submit/gbt_submitter.h"

static std::string getenv_str(const char* k) {
  const char* v = std::getenv(k);
  return v ? std::string(v) : std::string();
}

TEST(GBTRegtest, SmokeFetchTemplateAndBuildBlockHex) {
  // Env gated: set BMAD_REGTEST_RPC_URL (e.g., http://127.0.0.1:18443) to run
  std::string url = getenv_str("BMAD_REGTEST_RPC_URL");
  if (url.empty()) {
    GTEST_SKIP() << "BMAD_REGTEST_RPC_URL not set";
  }
  net::RpcClient rpc(url);
  // Optional: provide cookie path
  std::string cookie = getenv_str("BMAD_REGTEST_COOKIE");
  if (!cookie.empty()) rpc.setCookieAuthFile(cookie);

  // Fetch getblocktemplate with segwit rule
  nlohmann::json params = nlohmann::json::array();
  nlohmann::json p = nlohmann::json::object();
  p["rules"] = nlohmann::json::array({"segwit"});
  params.push_back(p);
  auto tmpl = rpc.call("getblocktemplate", params);
  ASSERT_TRUE(tmpl.has_value()) << "gbt failed: " << rpc.lastError();

  // Update submitter template and ensure coinbase present or synth possible
  config::PoolEntry::RpcConfig rc; rc.url = url; rc.auth = "cookie";
  submit::GbtSubmitter sub(rc);
  sub.setAllowSynthCoinbase(true);
  sub.updateTemplate(*tmpl);
  EXPECT_TRUE(sub.hasCoinbase());

  // Build a dummy header (all zeros) and ensure we can build block hex
  uint8_t header[80] = {0};
  std::string block_hex;
  ASSERT_TRUE(sub.buildBlockHex(header, &block_hex));
  ASSERT_GT(block_hex.size(), 162u);
}




