#include "obs/log.h"

#include <chrono>
#include <iostream>

namespace obs {

namespace {
const char* ToString(LogLevel level) {
  switch (level) {
    case LogLevel::kTrace: return "trace";
    case LogLevel::kDebug: return "debug";
    case LogLevel::kInfo:  return "info";
    case LogLevel::kWarn:  return "warn";
    case LogLevel::kError: return "error";
  }
  return "info";
}
}

Logger::Logger(std::string component) : component_(std::move(component)) {}

void Logger::log(LogLevel level, std::string_view message, const nlohmann::json& fields) {
  using clock = std::chrono::system_clock;
  const auto now = clock::now();
  const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();

  nlohmann::json j;
  j["ts_ms"] = ms;
  j["level"] = ToString(level);
  j["component"] = component_;
  j["msg"] = std::string(message);
  if (!fields.is_null() && !fields.empty()) {
    j["fields"] = fields;
  }
  std::cout << j.dump() << std::endl;
}

void Logger::info(std::string_view message, const nlohmann::json& fields) {
  log(LogLevel::kInfo, message, fields);
}
void Logger::warn(std::string_view message, const nlohmann::json& fields) {
  log(LogLevel::kWarn, message, fields);
}
void Logger::error(std::string_view message, const nlohmann::json& fields) {
  log(LogLevel::kError, message, fields);
}

}  // namespace obs


