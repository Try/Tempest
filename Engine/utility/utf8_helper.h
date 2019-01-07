#pragma once

#include <cstdlib>
#include <cstdint>
#include <cstring>

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

  private:
    const uint8_t* str;
    const uint8_t* beg;
    const uint8_t* end;
  };
}
