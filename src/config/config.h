#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace config {

struct PoolConfig {
  std::string url;
  std::string user;
  std::string pass_redacted;
};

struct AppConfig {
  int log_level{2}; // 0-trace,1-debug,2-info,3-warn,4-error
  std::vector<PoolConfig> pools;
};

// Placeholder loader; to be implemented with INI parser.
AppConfig loadDefault();

}


