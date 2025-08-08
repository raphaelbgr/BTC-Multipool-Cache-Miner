#pragma once

#include <string>
#include <string_view>
#include <nlohmann/json.hpp>

namespace obs {

enum class LogLevel {
  kTrace,
  kDebug,
  kInfo,
  kWarn,
  kError
};

class Logger {
 public:
  explicit Logger(std::string component);

  void log(LogLevel level, std::string_view message, const nlohmann::json& fields = {});

  void info(std::string_view message, const nlohmann::json& fields = {});
  void warn(std::string_view message, const nlohmann::json& fields = {});
  void error(std::string_view message, const nlohmann::json& fields = {});

 private:
  std::string component_;
};

}  // namespace obs


