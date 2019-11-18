#include <Tempest/Pixmap>
#include <Tempest/MemWriter>
#include <Tempest/MemReader>

#include <gtest/gtest.h>
#include <gmock/gmock-matchers.h>

using namespace testing;
using namespace Tempest;

TEST(main,PixmapIO_0) {
  Pixmap pm("data/img/tst.png");
  EXPECT_EQ(pm.w(),     256);
  EXPECT_EQ(pm.h(),     256);
  EXPECT_EQ(pm.format(),Pixmap::Format::RGBA);
  }

TEST(main,PixmapIO_1) {
  Pixmap pm("data/img/1.jpg");
  EXPECT_EQ(pm.w(),     852);
  EXPECT_EQ(pm.h(),     480);
  EXPECT_EQ(pm.format(),Pixmap::Format::RGB);
  }

TEST(main,PixmapIO_SymetricIO) {
  Pixmap pm("data/img/tst.png");

  static const char* frm[]={"png","jpg","tga","bmp"};
  for(auto f:frm) {
    std::vector<uint8_t> mem;
    MemWriter wr(mem);
    pm.save(wr,f);

    size_t realSz = mem.size();
    mem.push_back(0);
    MemReader rd(mem);
    pm = Pixmap(rd);

    EXPECT_EQ(realSz,rd.cursorPosition());
    }
  }
