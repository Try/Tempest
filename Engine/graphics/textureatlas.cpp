#include "textureatlas.h"

#include <Tempest/Sprite>

using namespace Tempest;

TextureAtlas::TextureAtlas(Device& device)
  :device(device),alloc(provider) {
  }

TextureAtlas::~TextureAtlas() {
  }

Sprite TextureAtlas::load(const Pixmap &pm) {
  auto a = alloc.alloc(int32_t(pm.w()),int32_t(pm.h()));
  emplace(a,pm,a.x,a.y);
  Sprite ret(a,pm.w(),pm.h());
  return ret;
  }

void TextureAtlas::emplace(TextureAtlas::Allocation &dest, const Pixmap &p, uint32_t x, uint32_t y) {
  Pixmap& cpu = *dest.page->memory;
  auto     data=reinterpret_cast<uint8_t*>(cpu.data());
  //uint32_t dbpp=4;//dest.cpu.bpp();
  uint32_t dx  =x*4;
  uint32_t dw  =cpu.w()*4;

  auto     src =reinterpret_cast<const uint8_t*>(p.data());
  uint32_t sbpp=p.bpp();
  uint32_t sw  =p.w()*sbpp;
  uint32_t sh  =p.h();//*sbpp;

  switch(p.format()) {
    case Pixmap::Format::RGBA: {
      for(uint32_t iy=0;iy<sh;++iy)
        memcpy(data+((y+iy)*dw+dx),src+iy*sw,sw);
      break;
      }
    case Pixmap::Format::RGB: {
      for(uint32_t iy=0;iy<sh;++iy){
        auto data0=data+((y+iy)*dw+dx);
        auto src0 =src+iy*sw;
        for(uint32_t ix=0,dx=0;ix<sw;dx+=4,ix+=3){
          data0[dx  ]=src0[ix  ];
          data0[dx+1]=src0[ix+1];
          data0[dx+2]=src0[ix+2];
          data0[dx+3]=255;
          }
        }
      break;
      }
    }

  cpu.save("dbg.png");
  }
