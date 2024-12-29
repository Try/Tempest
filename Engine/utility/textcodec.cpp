#include "textcodec.h"

#include "utf8_helper.h"

using namespace Tempest;

std::string TextCodec::toUtf8(std::u16string_view inS) {
  const uint16_t* s  = reinterpret_cast<const uint16_t*>(inS.data());

  size_t sz=0;
  for(size_t i=0; i<inS.size();) {
    uint32_t cp = 0;
    size_t   l  = Detail::utf16ToCodepoint(&s[i],cp);
    sz += Detail::codepointToUtf8(cp);
    i  += l;
    }

  std::string u(sz,'?');
  sz=0;
  for(size_t i=0; i<inS.size();) {
    uint32_t cp = 0;
    size_t   l  = Detail::utf16ToCodepoint(&s[i],cp);
    sz += Detail::codepointToUtf8(cp,&u[sz]);
    i  += l;
    }

  return u;
  }

std::string TextCodec::toUtf8(const char16_t *s) {
  return toUtf8(std::u16string_view(s));
  }

void TextCodec::toUtf8(const uint32_t codePoint, char (&ret)[3]) {
  size_t sz = Detail::codepointToUtf8(codePoint,ret);
  ret[sz] = '\0';
  }

std::u16string TextCodec::toUtf16(std::string_view inS) {
  const uint8_t* s  = reinterpret_cast<const uint8_t*>(inS.data());
  size_t         sz = 0;

  for(size_t i=0; i<inS.size();) {
    uint32_t cp = 0;
    size_t   l  = Detail::utf8ToCodepoint(&s[i],cp);
    if(l==0)
      throw std::range_error("invalid utf-8"); // similar to stl

    if(cp > 0xFFFF)
      sz+=2; else
      sz+=1;

    i+=l;
    }

  std::u16string u(sz,'?');
  size_t l = 0;
  for(size_t i=0; i<sz;) {
    uint32_t cp = 0;
    l += Detail::utf8ToCodepoint(&s[l],cp);

    if(cp > 0xFFFF) {
      cp  -= 0x10000;
      u[i] = 0xD800 + ((cp >> 10) & 0x3FF);
      cp   = 0xDC00 + (cp & 0x3FF);
      ++i;
      }
    u[i] = char16_t(cp);
    ++i;
    }

  return u;
  }

std::u16string TextCodec::toUtf16(const char *s) {
  return toUtf16(std::string_view(s));
  }
