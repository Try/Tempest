#include "sprite.h"

using namespace Tempest;

Sprite::Sprite() {
  }

Sprite::Sprite(TextureAtlas::Allocation a, uint32_t w, uint32_t h)
  :alloc(a),texW(w),texH(h) {
  }
