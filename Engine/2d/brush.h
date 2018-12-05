#pragma once

#include <Tempest/Texture2d>
#include <Tempest/Color>
#include <Tempest/Sprite>

namespace Tempest {

class Brush {
  public:
    Brush(const Tempest::Texture2d& texture);
    Brush(const Tempest::Color& color);
    Brush(const Tempest::Texture2d& texture,const Tempest::Color& color);
    Brush(const Tempest::Sprite&    texture);

    uint32_t w() const { return info.w; }
    uint32_t h() const { return info.h; }

  private:
    using TexPtr=Detail::ResourcePtr<Tempest::Texture2d>;
    TexPtr                      tex;
    Sprite                      spr;
    Tempest::Color              color;

    struct Info {
      uint32_t w=0,h=0;
      float    invW=0;
      float    invH=0;
      float    dx  =0;
      float    dy  =0;
      };
    Info info;

  friend class Painter;
  };

}
