#include <gtest/gtest.h>
#include <unordered_set>

#include "cache/vram_pages.h"
#include "cache/cache_manager.h"

TEST(Cache, VramPagesBasicLifecycle) {
  cache::VramPages pages;
  auto id1 = pages.create(64);
  auto id2 = pages.create(128);
  ASSERT_NE(id1, 0ull);
  ASSERT_NE(id2, 0ull);
  EXPECT_NE(id1, id2);

  EXPECT_TRUE(pages.markInUse(id1, true));
  EXPECT_TRUE(pages.markInUse(id1, false));
  EXPECT_FALSE(pages.markInUse(999999, true));

  // totalPages should reflect two pages
  EXPECT_EQ(pages.totalPages(), 2u);
  EXPECT_TRUE(pages.destroy(id1));
  EXPECT_EQ(pages.totalPages(), 1u);
  EXPECT_TRUE(pages.destroy(id2));
  EXPECT_EQ(pages.totalPages(), 0u);
  EXPECT_FALSE(pages.destroy(id2)); // already destroyed
}

TEST(Cache, CacheManagerAllocateReleaseAndWatermarks) {
  cache::CacheManager mgr({85, 256});
  auto wm = mgr.watermarks();
  EXPECT_EQ(wm.targetUtilPercent, 85u);
  EXPECT_EQ(wm.minFreeMiB, 256u);

  mgr.setWatermarks({90, 512});
  wm = mgr.watermarks();
  EXPECT_EQ(wm.targetUtilPercent, 90u);
  EXPECT_EQ(wm.minFreeMiB, 512u);

  auto id = mgr.allocate(32);
  ASSERT_NE(id, 0ull);
  EXPECT_TRUE(mgr.release(id));
  EXPECT_FALSE(mgr.release(id));
}

TEST(Cache, CacheManagerMultipleAllocationsUniqueIds) {
  cache::CacheManager mgr({85, 256});
  std::unordered_set<cache::VramPageId> ids;
  for (int i = 0; i < 10; ++i) {
    auto id = mgr.allocate(16);
    ASSERT_NE(id, 0ull);
    EXPECT_TRUE(ids.insert(id).second);
  }
  for (auto id : ids) {
    EXPECT_TRUE(mgr.release(id));
  }
}


