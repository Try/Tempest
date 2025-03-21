#pragma once

#include <string>
#include <cstdint>

namespace Tempest {

class TextCodec final {
  public:
    TextCodec()=delete;

    static std::string    toUtf8 (std::u16string_view s);
    static std::string    toUtf8 (const char16_t*     s);

    static void           toUtf8 (const uint32_t codePoint, char (&ret)[3]);

    static std::u16string toUtf16(std::string_view s);
    static std::u16string toUtf16(const char*      s);
  };

}

