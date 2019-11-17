#pragma once

#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <string>
#include <codecvt>
#include <locale>

namespace Tempest {
namespace Detail {

inline size_t utf8LetterLength(const uint8_t* str) {
  uint8_t lead = *str;
  if(lead < 0x80)
    return 1;
  if((lead >> 5) == 0x6)
    return 2;
  if((lead >> 4) == 0xe)
    return 3;
  if((lead >> 3) == 0x1e)
    return 4;
  return 0;
  }

inline size_t utf8LetterLength(const char* str) {
  return utf8LetterLength(reinterpret_cast<const uint8_t*>(str));
  }

inline uint32_t getChar1(const uint8_t* str) {
  return *str;
  }

inline uint32_t getChar2(const uint8_t* str) {
  uint32_t code_point = str[0];
  code_point = ((code_point << 6) & 0x7ff) + (str[1] & 0x3f);
  return code_point;
  }

inline uint32_t getChar3(const uint8_t* str) {
  uint32_t code_point = str[0];
  code_point = ((code_point << 12) & 0xffff) + ((str[1] << 6) & 0xfff);
  code_point += (str[2]) & 0x3f;
  return code_point;
  }

inline uint32_t getChar4(const uint8_t* str) {
  uint32_t code_point = str[0];
  code_point = ((code_point << 18) & 0x1fffff) + ((str[1] << 12) & 0x3ffff);
  code_point += (str[2] << 6) & 0xfff;
  code_point += str[3] & 0x3f;
  return code_point;
  }

inline uint8_t codepointToUtf8(uint32_t cp) {
  if(cp > 0x10FFFF || cp == 0xFFFE || cp == 0xFFFF) {
    return 1;
    } else {
    // There are seven "UTF-16 surrogates" that are illegal in UTF-8.
    switch(cp) {
      case 0xD800:
      case 0xDB7F:
      case 0xDB80:
      case 0xDBFF:
      case 0xDC00:
      case 0xDF80:
      case 0xDFFF:
        return 1;
      }
    }

  if(cp < 0x80)
    return 1;
  if(cp < 0x800)
    return 2;
  if(cp < 0x10000)
    return 3;
  return 4;
  }

inline uint8_t codepointToUtf8(uint32_t cp, char *dst) {
  if(cp > 0x10FFFF || cp == 0xFFFE || cp == 0xFFFF) {
    cp = '?';
    } else {
    // There are seven "UTF-16 surrogates" that are illegal in UTF-8.
    switch(cp) {
      case 0xD800:
      case 0xDB7F:
      case 0xDB80:
      case 0xDBFF:
      case 0xDC00:
      case 0xDF80:
      case 0xDFFF:
        cp = '?';
      }
    }

  if(cp < 0x80) {
    dst[0] = char(cp);
    return 1;
    }

  if(cp < 0x800) {
    dst[0] = char((cp >> 6) | 128 | 64);
    dst[1] = char(cp & 0x3F) | 128;
    return 2;
    }

  if(cp < 0x10000) {
    dst[0] = char((cp >> 12) | 128 | 64 | 32);
    dst[1] = char((cp >> 6) & 0x3F) | 128;
    dst[2] = char(cp & 0x3F) | 128;
    return 3;
    }

  dst[0] = char((cp >> 18) | 128 | 64 | 32 | 16);
  dst[1] = char((cp >> 12) & 0x3F) | 128;
  dst[2] = char((cp >> 6) & 0x3F) | 128;
  dst[3] = char(cp & 0x3F) | 128;
  return 4;
  }

inline uint8_t utf8ToCodepoint(const uint8_t* str,uint32_t& cp) {
  uint8_t lead = *str;
  if(lead < 0x80) {
    cp = getChar1(str);
    return 1;
    }
  if((lead >> 5) == 0x6) {
    cp = getChar2(str);
    return 2;
    }
  if((lead >> 4) == 0xe) {
    cp = getChar3(str);
    return 3;
    }
  if((lead >> 3) == 0x1e) {
    cp = getChar4(str);
    return 4;
    }
  return 0;
  }

inline uint8_t utf16ToCodepoint(const uint16_t* str,uint32_t& cp) {
  uint16_t lead = *str;
  if(0xd800u<=lead && lead<=0xdbffu) {
    uint32_t next = str[1];
    cp = 0x10000 + (uint32_t(lead - 0xD800) << 10) + (next - 0xDC00);
    return 2;
    }
  cp = lead;
  return 1;
  }
}

class Utf8Iterator {
  public:
    Utf8Iterator(const char* str):Utf8Iterator(str,str==nullptr ? 0 :  std::strlen(str)){}
    Utf8Iterator(const char* str,size_t len)
      :str(reinterpret_cast<const uint8_t*>(str)),beg(this->str),end(this->str+len){}

    char32_t operator*() const {
      const auto l = Detail::utf8LetterLength(str);
      if(str+l>end)
        return '\0';

      switch(l){
        case 1:  return Detail::getChar1(str);
        case 2:  return Detail::getChar2(str);
        case 3:  return Detail::getChar3(str);
        case 4:  return Detail::getChar4(str);
        }
      return '\0';
      }

    char32_t peek() const {
      const auto l = Detail::utf8LetterLength(str);

      if(str+l>end){
        return '\0';
        }

      switch(l){
        case 1: return Detail::getChar1(str);
        case 2: return Detail::getChar2(str);
        case 3: return Detail::getChar3(str);
        case 4: return Detail::getChar4(str);
        }
      return '\0';
      }

    char32_t next(){
      const auto l = Detail::utf8LetterLength(str);

      if(str+l>end){
        str=end;
        return '\0';
        }

      char32_t ch;
      switch(l){
        case 1:
          ch=Detail::getChar1(str);
          str+=1;
          return ch;
        case 2:
          ch=Detail::getChar2(str);
          str+=2;
          return ch;
        case 3:
          ch=Detail::getChar3(str);
          str+=3;
          return ch;
        case 4:
          ch=Detail::getChar4(str);
          str+=4;
          return ch;
        }
      return '\0';
      }

    size_t pos() const {
      return size_t(str-beg);
      }

    bool hasData() const {
      return str<end;
      }

    void operator ++ () {
      str += Detail::utf8LetterLength(str);
      }

    bool operator == (const Utf8Iterator& other) const {
      return str==other.str &&
             beg==other.beg &&
             end==other.end;
      }

    bool operator != (const Utf8Iterator& other) const {
      return str!=other.str ||
             beg!=other.beg ||
             end!=other.end;
      }

  private:
    const uint8_t* str;
    const uint8_t* beg;
    const uint8_t* end;
  };
}
