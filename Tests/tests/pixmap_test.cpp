#include <Tempest/Pixmap>
#include <Tempest/MemWriter>
#include <Tempest/MemReader>

#include <gtest/gtest.h>
#include <gmock/gmock-matchers.h>

using namespace testing;
using namespace Tempest;

TEST(main,PixmapFormatRG16F) {
  EXPECT_EQ(uint8_t(TextureFormat::RGBA16F),25);
  EXPECT_STREQ(formatName(TextureFormat::RG16F),"RG16F");

  Pixmap pm(3,2,TextureFormat::RG16F);
  EXPECT_EQ(pm.format(),TextureFormat::RG16F);
  EXPECT_EQ(pm.bpp(),4);
  EXPECT_EQ(pm.dataSize(),24);
  EXPECT_EQ(Pixmap::componentCount(TextureFormat::RG16F),2);

  const auto blocks = Pixmap::blockCount(TextureFormat::RG16F,3,2);
  EXPECT_EQ(blocks.w,3);
  EXPECT_EQ(blocks.h,2);
  }

TEST(main,PixmapIO_0) {
  Pixmap pm("assets/pixmap_io/rgba.png");
  EXPECT_EQ(pm.w(),     256);
  EXPECT_EQ(pm.h(),     256);
  EXPECT_EQ(pm.format(),TextureFormat::RGBA8);
  }

TEST(main,PixmapIO_1) {
  Pixmap pm("assets/pixmap_io/rgb.jpg");
  EXPECT_EQ(pm.w(),     852);
  EXPECT_EQ(pm.h(),     480);
  EXPECT_EQ(pm.format(),TextureFormat::RGB8);
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
  EXPECT_EQ(pm.format(),TextureFormat::DXT5);

  Pixmap px0(pm,TextureFormat::RGB8);
  EXPECT_EQ(px0.format(),TextureFormat::RGB8);
  px0.save("tst-dxt5.png");

  Pixmap px1(px0,TextureFormat::RGBA16);
  EXPECT_EQ(px1.format(),TextureFormat::RGBA16);
  px1.save("tst-dxt5.png");
  }
