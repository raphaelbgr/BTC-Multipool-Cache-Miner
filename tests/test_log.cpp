#include <gtest/gtest.h>
#include <sstream>
#include <iostream>
#include "obs/log.h"

TEST(Log, JsonShape) {
  obs::Logger log("test");
  testing::internal::CaptureStdout();
  log.info("hello", nlohmann::json{{"k", 1}});
  std::string out = testing::internal::GetCapturedStdout();
  // Expect minimal JSON fields
  EXPECT_NE(out.find("\"level\":"), std::string::npos);
  EXPECT_NE(out.find("\"component\":"), std::string::npos);
  EXPECT_NE(out.find("\"msg\":"), std::string::npos);
}


