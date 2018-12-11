#pragma once

#include <Tempest/Texture2d>
#include <Tempest/Sprite>
#include <Tempest/Color>
#include <Tempest/PaintDevice>

namespace Tempest {

class Brush {
  public:
    Brush();
    Brush(const Tempest::Texture2d& texture,PaintDevice::Blend b=PaintDevice::Alpha);
    Brush(const Tempest::Color&     color,  PaintDevice::Blend b=PaintDevice::Alpha);
    Brush(const Tempest::Texture2d& texture,const Tempest::Color& color,PaintDevice::Blend b=PaintDevice::Alpha);
    Brush(const Tempest::Sprite&    texture,PaintDevice::Blend b=PaintDevice::Alpha);

    uint32_t w() const { return info.w; }
    uint32_t h() const { return info.h; }

  private:
    using TexPtr=Detail::ResourcePtr<Tempest::Texture2d>;
    TexPtr                      tex;
    Sprite                      spr;
    Tempest::Color              color;
    PaintDevice::Blend          blend=PaintDevice::NoBlend;

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
