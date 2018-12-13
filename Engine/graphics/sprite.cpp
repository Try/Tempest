#include "sprite.h"

#include <Tempest/Device>

using namespace Tempest;

Sprite::Sprite() {
  }

Sprite::Sprite(TextureAtlas::Allocation a, uint32_t w, uint32_t h)
  :alloc(std::move(a)),texW(w),texH(h) {
  }

const Texture2d &Sprite::pageRawData(Device& dev) const {
  if(!alloc.owner){
    static const Texture2d t;
    return t;
    }

  auto& mem=alloc.memory();
  if( mem.changed ){
    mem.gpu=dev.loadTexture(mem.cpu,false);
    mem.changed=false;
    }
  return mem.gpu;
  }

const Rect Sprite::pageRect() const {
  if(alloc.owner){
    return alloc.pageRect();
    }
  static Rect r;
  return r;
  }

void *Sprite::pageId() const {
  return reinterpret_cast<void*>(uintptr_t(alloc.pageId()));
  }
