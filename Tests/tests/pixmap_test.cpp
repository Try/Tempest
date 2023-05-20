#include <Tempest/Pixmap>
#include <Tempest/MemWriter>
#include <Tempest/MemReader>

#include <gtest/gtest.h>
#include <gmock/gmock-matchers.h>

using namespace testing;
using namespace Tempest;

TEST(main,PixmapIO_0) {
  Pixmap pm("assets/pixmap_io/rgba.png");
  EXPECT_EQ(pm.w(),     256);
  EXPECT_EQ(pm.h(),     256);
  EXPECT_EQ(pm.format(),Pixmap::Format::RGBA);
  }

TEST(main,PixmapIO_1) {
  Pixmap pm("assets/pixmap_io/rgb.jpg");
  EXPECT_EQ(pm.w(),     852);
  EXPECT_EQ(pm.h(),     480);
  EXPECT_EQ(pm.format(),Pixmap::Format::RGB);
  }

TEST(main,PixmapIO_SymetricIO) {
  Pixmap pm("assets/pixmap_io/rgba.png");

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

TEST(main,PixmapConv) {
  Pixmap pm("assets/pixmap_io/dxt5.dds");
  EXPECT_EQ(pm.w(),     512);
  EXPECT_EQ(pm.h(),     512);
  EXPECT_EQ(pm.format(),Pixmap::Format::DXT5);

  Pixmap px0(pm,Pixmap::Format::RGB);
  EXPECT_EQ(px0.format(),Pixmap::Format::RGB);
  px0.save("tst-dxt5.png");

  Pixmap px1(px0,Pixmap::Format::RGBA16);
  EXPECT_EQ(px1.format(),Pixmap::Format::RGBA16);
  px1.save("tst-dxt5.png");
  }
