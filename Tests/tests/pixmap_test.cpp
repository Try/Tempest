#include <Tempest/Pixmap>

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
