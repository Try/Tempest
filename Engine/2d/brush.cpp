#include "brush.h"

using namespace Tempest;

Brush::Brush(const Texture2d &texture)
  :tex(texture),color(1.f) {
  info.w=texture.w();
  info.h=texture.h();
  }

Brush::Brush(const Texture2d &texture, const Color &color)
  :tex(texture),color(color) {
  info.w=texture.w();
  info.h=texture.h();
  }
