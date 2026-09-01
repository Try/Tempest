#include <Tempest/Application>
#include <Tempest/Font>

#include <gtest/gtest.h>

using namespace Tempest;

TEST(FontTest, MissingPlatformFallbackIsSafe) {
  Font font = Application::defaultFont();

  // Roboto represents U+200B as a zero-advance, empty glyph. Platforms without
  // a configured fallback must keep it empty instead of passing a null buffer
  // to stb_truetype.
  Size size;
  EXPECT_NO_THROW(size = font.textSize("\xE2\x80\x8B"));
#ifndef __WINDOWS__
  // Windows supplies Georgia as a fallback, so its resulting metrics are
  // font-dependent.
  EXPECT_EQ(size.w,0);
  EXPECT_EQ(size.h,0);
#endif
  }
