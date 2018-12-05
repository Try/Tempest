#include "sprite.h"

#include <Tempest/Device>

using namespace Tempest;

Sprite::Sprite() {
  }

Sprite::Sprite(TextureAtlas::Allocation a, uint32_t w, uint32_t h)
  :alloc(std::move(a)),texW(w),texH(h) {
  }

bool Sprite::operator==(const Sprite &s) const {
  return s.alloc.page==alloc.page && s.alloc.id==alloc.id;
  }

bool Sprite::operator!=(const Sprite &s) const {
  return s.alloc.page!=alloc.page || s.alloc.id!=alloc.id;
  }

const Texture2d &Sprite::pageRawData(Device& dev) const {
  if(!alloc.page){
    static const Texture2d t;
    return t;
    }

  auto& mem=alloc.page->memory;
  if( mem.changed ){
    mem.gpu=dev.loadTexture(mem.cpu,false);
    mem.changed=false;
    }
  return mem.gpu;
  }

const Rect Sprite::pageRect() const {
  if(alloc.page){
    auto& node=alloc.page->node[alloc.id];
    return Rect(int(node.x),int(node.y),int(alloc.page->w),int(alloc.page->h));
    }
  static Rect r;
  return r;
  }
