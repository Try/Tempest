#pragma once

#include <Tempest/File>
#include <Tempest/Point>
#include <Tempest/Sprite>

#include <string>
#include <memory>

namespace Tempest {

class FontElement final {
  public:
    FontElement();
    FontElement(const char* file);

    class LetterGeometry final {
      public:
        Tempest::Size  size;
        Tempest::Point dpos, advance;
      };

    class Letter final {
      public:
        Tempest::Size   size;
        Tempest::Point  dpos, advance;
        Tempest::Sprite view;
      };

    const Letter& letter(char16_t ch,float size,TextureAtlas& tex) const;

  private:
    struct Impl;
    std::shared_ptr<Impl> ptr;
  };

class Font final {
  public:
    using LetterGeometry = FontElement::LetterGeometry;
    using Letter         = FontElement::Letter;

    Font()=default;
    Font(const char* file);

    void setSize(float size);

    const Letter& letter(char16_t ch,TextureAtlas& tex) const;

  private:
    FontElement fnt[2][2];
    float       size=16.f;
  };
}
