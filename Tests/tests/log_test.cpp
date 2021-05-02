#include <Tempest/Log>

#include <gtest/gtest.h>
#include <gmock/gmock-matchers.h>

#include <cstring>

using namespace testing;
using namespace Tempest;

TEST(main,Logger) {
  size_t s = 0;
  Log::i(s);

  uint64_t u64 = 0;
  Log::i(u64);

  int64_t i64 = 0;
  Log::i(i64);

  uint32_t u32 = 0;
  Log::i(u32);

  int64_t i32 = 0;
  Log::i(i32);
  }
