#include "brush.h"

using namespace Tempest;

Brush::Brush()
  :color(1.f){
  }

Brush::Brush(const Texture2d &texture, PaintDevice::Blend b)
  :tex(texture),texFrm(texture.format()),color(1.f),blend(b) {
  info.w    = texture.w();
  info.h    = texture.h();
  info.invW = 1.f/info.w;
  info.invH = 1.f/info.h;
  }

Brush::Brush(const Color &color, PaintDevice::Blend b)
  :color(color),blend(b) {
  info.w    = 1;
  info.h    = 1;
  info.invW = 1.f;
  info.invH = 1.f;
  }

Brush::Brush(const Texture2d &texture, const Color &color,PaintDevice::Blend b)
  :tex(texture),texFrm(texture.format()),color(color),blend(b) {
  info.w    = texture.w();
  info.h    = texture.h();
  info.invW = 1.f/info.w;
  info.invH = 1.f/info.h;
  }

Brush::Brush(const Sprite &texture, PaintDevice::Blend b)
  :spr(texture),color(1.f),blend(b) {
  auto r = spr.pageRect();

  info.w    = texture.w();
  info.h    = texture.h();
  info.invW = 1.f/r.w;
  info.invH = 1.f/r.h;
  info.dx   = r.x*info.invW;
  info.dy   = r.y*info.invH;
  }

Brush::Brush(const Sprite &texture, const Color &color, PaintDevice::Blend b)
  :spr(texture),color(color),blend(b) {
  auto r = spr.pageRect();

  info.w    = texture.w();
  info.h    = texture.h();
  info.invW = 1.f/r.w;
  info.invH = 1.f/r.h;
  info.dx   = r.x*info.invW;
  info.dy   = r.y*info.invH;
  }
