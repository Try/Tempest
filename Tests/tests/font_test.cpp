#include <Tempest/Application>
#include <Tempest/Font>

#include <gtest/gtest.h>

using namespace Tempest;

TEST(FontTest, MissingPlatformFallbackIsSafe) {
  Font font = Application::defaultFont();

  // U+10FFFF is intentionally absent from the bundled Roboto font. Platforms
  // without a configured system fallback must return empty geometry instead
  // of trying to initialize stb_truetype with a null buffer.
  const Size size = font.textSize("\xF4\x8F\xBF\xBF");
  EXPECT_GE(size.w,0);
  EXPECT_GE(size.h,0);
  }
