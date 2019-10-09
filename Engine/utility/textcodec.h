#pragma once

#include <string>

namespace Tempest {

class TextCodec final {
  public:
    TextCodec()=delete;

    static std::string    toUtf8 (const std::u16string& s);
    static std::string    toUtf8 (const char16_t* s);

    static std::u16string toUtf16(const std::string& s);
    static std::u16string toUtf16(const char* s);
  };

}

