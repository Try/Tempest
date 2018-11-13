#include "brush.h"

using namespace Tempest;

Brush::Brush(const Texture2d &texture)
  :tex(texture){
  info.w=texture.w();
  info.h=texture.h();
  }
