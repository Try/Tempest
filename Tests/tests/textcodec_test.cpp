#include <Tempest/File>
#include <Tempest/MemWriter>
#include <Tempest/TextCodec>

#include <gtest/gtest.h>
#include <gmock/gmock-matchers.h>

#include <cstring>

using namespace testing;
using namespace Tempest;

static void TextCodec_Base(const std::string& u8,const std::u16string& u16){
  auto cvt8  = TextCodec::toUtf8(u16);
  EXPECT_EQ(cvt8,u8);

  auto cvt16 = TextCodec::toUtf16(u8);
  EXPECT_EQ(cvt16,u16);
  }

TEST(main,TextCodec_UTF8_0) {
  std::string    u8  =  "latin 01234567890_";
  std::u16string u16 = u"latin 01234567890_";

  TextCodec_Base(u8,u16);
  }

TEST(main,TextCodec_UTF8_1) {
  std::string    u8  =  "z\u00df\u6c34";
  std::u16string u16 = u"z\u00df\u6c34";

  TextCodec_Base(u8,u16);
  }

TEST(main,TextCodec_UTF8_2) {
  std::string    u8  =  "\U0001f34c";
  std::u16string u16 = u"\U0001f34c";

  TextCodec_Base(u8,u16);
  }

TEST(main,TextCodec_UTF8_3) {
  std::string    u8  =  "z\u00df\u6c34\U0001f34c";
  std::u16string u16 = u"z\u00df\u6c34\U0001f34c";

  TextCodec_Base(u8,u16);
  }
