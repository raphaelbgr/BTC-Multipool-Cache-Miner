#include <gtest/gtest.h>

#include "adapters/pool_profiles.h"

TEST(PoolProfiles, GenericFormatsWalletWorker) {
  auto [user, pass] = adapters::formatStratumCredentials(adapters::PoolProfile::kGeneric,
                                                         "bc1qexampleaddr", "rig1", "x");
  EXPECT_EQ(user, std::string("bc1qexampleaddr.rig1"));
  EXPECT_EQ(pass, std::string("x"));
}

TEST(PoolProfiles, EmptyWorkerOmitsDot) {
  auto [user, pass] = adapters::formatStratumCredentials(adapters::PoolProfile::kGeneric,
                                                         "bc1qexampleaddr", "", "");
  EXPECT_EQ(user, std::string("bc1qexampleaddr"));
  EXPECT_EQ(pass, std::string("x"));
}



