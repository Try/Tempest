#include <Tempest/File>
#include <Tempest/MemWriter>
#include <Tempest/MemReader>

#include <gtest/gtest.h>
#include <gmock/gmock-matchers.h>

#include <cstring>

using namespace testing;
using namespace Tempest;

static const uint8_t bytes[]={0,1,2,3,4,5,6,7,9,10,11};

TEST(main,FileIO) {
  {
  WFile fout("tmp.bin");
  EXPECT_EQ(fout.write(bytes,sizeof(bytes)),sizeof(bytes));
  }

  {
  char buf[sizeof(bytes)]={};
  RFile fin("tmp.bin");
  EXPECT_EQ(fin.read(buf,sizeof(buf)),sizeof(buf));
  EXPECT_EQ(std::memcmp(buf,bytes,sizeof(bytes)),0);
  }

  {
  // shared read
  RFile f0("tmp.bin");
  RFile f1("tmp.bin");
  }
  }

TEST(main,MemoryIO) {
  std::vector<uint8_t> tmp;
  {
  MemWriter fout(tmp);
  EXPECT_EQ(fout.write(bytes,sizeof(bytes)),sizeof(bytes));
  EXPECT_EQ(tmp.size(),sizeof(bytes));
  }

  {
  char buf[sizeof(bytes)]={};
  MemReader fin(tmp);
  EXPECT_EQ(fin.read(buf,sizeof(buf)),sizeof(buf));
  EXPECT_EQ(std::memcmp(buf,bytes,sizeof(bytes)),0);
  }
  }
