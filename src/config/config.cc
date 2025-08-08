#include "config/config.h"

namespace config {

AppConfig loadDefault() {
  AppConfig cfg;
  cfg.log_level = 2;
  return cfg;
}

}


