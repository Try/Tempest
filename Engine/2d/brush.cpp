#include "brush.h"

using namespace Tempest;

Brush::Brush(const Texture2d &texture)
  :tex(texture),color(1.f) {
  info.w    = texture.w();
  info.h    = texture.h();
  info.invW = 1.f/info.w;
  info.invH = 1.f/info.h;
  }

Brush::Brush(const Color &color)
  :color(color){
  info.w    = 1;
  info.h    = 1;
  info.invW = 1.f;
  info.invH = 1.f;
  }

Brush::Brush(const Texture2d &texture, const Color &color)
  :tex(texture),color(color) {
  info.w    = texture.w();
  info.h    = texture.h();
  info.invW = 1.f/info.w;
  info.invH = 1.f/info.h;
  }

Brush::Brush(const Sprite &texture)
  :spr(texture),color(1.f) {
  auto r = spr.pageRect();

  info.w    = texture.w();
  info.h    = texture.h();
  info.invW = 1.f/r.w;
  info.invH = 1.f/r.h;
  info.dx   = r.x*info.invW;
  info.dy   = r.y*info.invH;
  }
