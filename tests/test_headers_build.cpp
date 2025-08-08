#include <gtest/gtest.h>
#include "obs/log.h"
#include "normalize/endianness.h"
#include "normalize/targets.h"
#include "registry/work_source_registry.h"
#include "config/config.h"
#include "cuda/engine.h"

TEST(HeadersBuild, Presence) {
  // Ensure headers compile and the basic symbols are present.
  obs::Logger l("x");
  auto t = normalize::compactToTargetU32LE(0);
  registry::WorkSourceRegistry reg(1);
  auto cfg = config::loadDefault();
  (void)l; (void)t; (void)reg; (void)cfg;
  EXPECT_TRUE(true);
}


