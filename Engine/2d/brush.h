#pragma once

#include <Tempest/Texture2d>

namespace Tempest {

class Brush {
  public:
    Brush(const Tempest::Texture2d& texture);

    uint32_t w() const { return info.w; }
    uint32_t h() const { return info.h; }

  private:
    using TexPtr=Detail::ResourcePtr<Tempest::Texture2d>;
    TexPtr                      tex;

    struct Info {
      uint32_t w=0,h=0;
      };
    Info info;

  friend class Painter;
  };

}
