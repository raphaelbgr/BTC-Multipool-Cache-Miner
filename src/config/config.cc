#include "config/config.h"

#include <fstream>
#include <nlohmann/json.hpp>

namespace config {

static CredMode parseCredMode(const std::string& s) {
  if (s == "account_worker") return CredMode::AccountWorker;
  return CredMode::WalletAsUser;
}

AppConfig loadFromJsonFile(const std::string& path) {
  AppConfig cfg;
  std::ifstream ifs(path);
  if (!ifs.good()) return cfg;
  nlohmann::json j; try { ifs >> j; } catch (...) { return cfg; }
  if (j.contains("log_level")) cfg.log_level = j["log_level"].get<int>();
  if (j.contains("pools") && j["pools"].is_array()) {
    for (const auto& p : j["pools"]) {
      PoolEntry e;
      if (p.contains("name")) e.name = p["name"].get<std::string>();
      if (p.contains("profile")) e.profile = p["profile"].get<std::string>();
      if (p.contains("cred_mode")) e.cred_mode = parseCredMode(p["cred_mode"].get<std::string>());
      if (p.contains("wallet")) e.wallet = p["wallet"].get<std::string>();
      if (p.contains("account")) e.account = p["account"].get<std::string>();
      if (p.contains("worker")) e.worker = p["worker"].get<std::string>();
      if (p.contains("password")) e.password = p["password"].get<std::string>();
      if (p.contains("endpoints") && p["endpoints"].is_array()) {
        for (const auto& ep : p["endpoints"]) {
          Endpoint ed;
          if (ep.contains("host")) ed.host = ep["host"].get<std::string>();
          if (ep.contains("port")) ed.port = static_cast<uint16_t>(ep["port"].get<int>());
          if (ep.contains("use_tls")) ed.use_tls = ep["use_tls"].get<bool>();
          e.endpoints.push_back(ed);
        }
      }
      cfg.pools.push_back(std::move(e));
    }
  }
  return cfg;
}

}


