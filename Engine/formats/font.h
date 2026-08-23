#pragma once

#include <Tempest/File>
#include <Tempest/Point>
#include <Tempest/Sprite>

#include <string_view>
#include <memory>

namespace Tempest {

class Painter;

class FontElement final {
  public:
    FontElement();
    FontElement(std::nullptr_t);
    FontElement(std::string_view    file);
    FontElement(std::u16string_view file);
    FontElement(const void* data, size_t size);

    class LetterGeometry final {
      public:
        Tempest::Size  size;
        Tempest::Point dpos, advance;
      };

    class Metrics final {
      public:
        int ascent =0;
        int descent=0;
      };

    class Letter final {
      public:
        Tempest::Size   size;
        Tempest::Point  dpos, advance;
        Tempest::Sprite view;
        bool            hasView=false;
      };

    const LetterGeometry& letterGeometry(char32_t ch, float size) const;
    const Letter&         letter(char32_t ch, float size, TextureAtlas& tex) const;

    Size                  textSize(std::string_view text, float fontSize) const;
    Size                  textSize(std::string_view text, int maxW, float fontSize) const;
    bool                  isEmpty() const;

    Metrics               metrics(float size) const;

  private:
    struct LetterTable;
    struct Impl;
    std::shared_ptr<Impl> ptr;
  };

class Font final {
  public:
    using LetterGeometry = FontElement::LetterGeometry;
    using Letter         = FontElement::Letter;
    using Metrics        = FontElement::Metrics;

    Font()=default;
    Font(std::string_view    file);
    Font(std::u16string_view file);
    Font(const FontElement& regular, const FontElement& bold,
         const FontElement& italic,  const FontElement& boldItalic);

    void  setPixelSize(float size);
    float pixelSize() const { return size; }

    void  setBold(bool b);
    bool  isBold() const;

    void  setItalic(bool i);
    bool  isItalic() const;

    bool  isEmpty() const;

    Metrics               metrics() const;

    const LetterGeometry& letterGeometry(char16_t ch) const;
    const LetterGeometry& letterGeometry(char32_t ch) const;
    const Letter&         letter(char16_t ch,TextureAtlas& tex) const;
    const Letter&         letter(char32_t ch,TextureAtlas& tex) const;

    const Letter&         letter(char16_t ch,Painter& tex) const;
    const Letter&         letter(char32_t ch,Painter& tex) const;

    Size                  textSize(std::string_view text) const;
    Size                  textSize(int maxW, std::string_view text) const;

  private:
    template<class String>
    Font(String file, std::true_type);

    FontElement fnt[2][2];
    float       size   = 18.f;
    uint8_t     bold   = 0;
    uint8_t     italic = 0;
  };
}
