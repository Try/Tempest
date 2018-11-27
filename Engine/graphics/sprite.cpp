#include "sprite.h"

using namespace Tempest;

Sprite::Sprite() {
  }

Sprite::Sprite(TextureAtlas::Page *page, uint32_t x, uint32_t y, uint32_t w, uint32_t h)
  :page(page),texW(w),texH(h) {
  }
