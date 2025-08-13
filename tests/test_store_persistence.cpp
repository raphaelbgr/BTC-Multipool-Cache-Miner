#include <gtest/gtest.h>
#include <cstdio>
#include <filesystem>

#include "store/ledger.h"
#include "store/outbox.h"

TEST(Store, LedgerSaveLoad) {
  store::Ledger l;
  registry::WorkItem w{};
  w.work_id = 123;
  w.source_id = 9;
  w.version = 0x20000000u;
  w.ntime = 42;
  w.nbits = 0x1d00ffffu;
  w.prevhash_le[0] = 1;
  w.merkle_root_le[0] = 2;
  w.share_target_le[0] = 3;
  w.block_target_le[0] = 4;
  w.vmask = 0x1fffe000u;
  w.extranonce2_size = 4;
  w.clean_jobs = true;
  w.active = true;
  l.put(w);

  const char* path = "ledger_test.jsonl";
  ASSERT_TRUE(l.saveToFile(path));

  store::Ledger l2;
  ASSERT_TRUE(l2.loadFromFile(path));
  auto got = l2.get(123);
  ASSERT_TRUE(got.has_value());
  EXPECT_EQ(got->source_id, 9u);
  EXPECT_TRUE(got->clean_jobs);
}

TEST(Store, OutboxAppendAndLoad) {
  store::Outbox ob;
  store::PendingSubmit s{};
  s.work_id = 55;
  s.nonce = 0xdeadbeef;
  for (int i=0;i<80;++i) s.header80[i] = static_cast<uint8_t>(i);
  const char* path = "outbox_test.bin";
  ASSERT_TRUE(ob.appendToFile(path, s));

  store::Outbox ob2;
  ASSERT_TRUE(ob2.loadFromFile(path));
  auto d = ob2.tryDequeue();
  ASSERT_TRUE(d.has_value());
  EXPECT_EQ(d->work_id, 55ull);
  EXPECT_EQ(d->nonce, 0xdeadbeefu);
  EXPECT_EQ(d->header80[0], 0);
  EXPECT_EQ(d->header80[79], 79);
}

TEST(Store, OutboxRotateFile) {
  store::Outbox ob;
  const char* src = "outbox_rotate_src.bin";
  const char* dst = "outbox_rotate_dst.bin";
  // Write one record
  store::PendingSubmit s{}; s.work_id = 1; s.nonce = 2; for (int i=0;i<80;++i) s.header80[i]=0xAA;
  ASSERT_TRUE(ob.appendToFile(src, s));
  // Rotate
  ASSERT_TRUE(ob.rotateFile(src, dst));
  // Destination must exist and be at least one record in size
  std::error_code ec;
  auto sz_dst = std::filesystem::file_size(dst, ec);
  ASSERT_FALSE(ec);
  EXPECT_GE(static_cast<long long>(sz_dst), 92);
  // Source may be missing (rename) or truncated (copy+truncate). Accept either.
  if (std::filesystem::exists(src)) {
    auto sz_src = std::filesystem::file_size(src, ec);
    ASSERT_FALSE(ec);
    EXPECT_EQ(static_cast<long long>(sz_src), 0);
  }
}


