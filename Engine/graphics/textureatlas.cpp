#include "textureatlas.h"

#include <Tempest/Sprite>
#include <Tempest/Log>
#include <cstring>
#include <squish.h>

using namespace Tempest;

TextureAtlas::TextureAtlas(Device& device)
  :device(device),alloc(provider) {
  }

TextureAtlas::~TextureAtlas() {
  }

Sprite TextureAtlas::load(const Pixmap &pm) {
  return load(pm.data(),pm.w(),pm.h(),pm.format());
  }

Sprite TextureAtlas::load(const void *data, uint32_t w, uint32_t h, TextureFormat format) {
  auto a = alloc.alloc(w,h);
  auto p = a.pos();
  emplace(a,data,w,h,format,uint32_t(p.x),uint32_t(p.y));
  Sprite ret(std::move(a),w,h);
  return ret;
  }

void TextureAtlas::emplace(TextureAtlas::Allocation &dest, const void* img,
                           uint32_t pw, uint32_t ph, TextureFormat format,
                           uint32_t x, uint32_t y) {
  dest.memory().changed=true;
  Pixmap&  cpu  = dest.memory().cpu;
  auto     data = reinterpret_cast<uint8_t*>(cpu.data());
  uint32_t dx   = x*4;
  uint32_t dw   = cpu.w()*4;

  auto     src  = reinterpret_cast<const uint8_t*>(img);
  uint32_t sbpp = uint32_t(Pixmap::bppForFormat(format));
  uint32_t sw   = pw*sbpp;
  uint32_t sh   = ph;

  switch(format) {
    case TextureFormat::Undefined:
      break;
    case TextureFormat::DXT1:
    case TextureFormat::DXT3:
    case TextureFormat::DXT5:{
      Log::d("compressed sprites are not implemented");
      break;
      }
    case TextureFormat::RGBA16: {
      for(uint32_t iy=0;iy<sh;++iy){
        auto data0=data+((y+iy)*dw+dx);
        auto src0 =reinterpret_cast<const uint16_t*>(src+iy*sw);
        for(uint32_t ix=0,dx=0;ix<sw;dx+=4,ix+=4){
          data0[dx  ]=src0[ix  ]/256;
          data0[dx+1]=src0[ix+1]/256;
          data0[dx+2]=src0[ix+2]/256;
          data0[dx+3]=src0[ix+3]/256;
          }
        }
      break;
      }
    case TextureFormat::RGB16: {
      for(uint32_t iy=0;iy<sh;++iy){
        auto data0=data+((y+iy)*dw+dx);
        auto src0 =reinterpret_cast<const uint16_t*>(src+iy*sw);
        for(uint32_t ix=0,dx=0;ix<sw;dx+=4,ix+=3){
          data0[dx  ]=src0[ix  ]/256;
          data0[dx+1]=src0[ix+1]/256;
          data0[dx+2]=src0[ix+2]/256;
          data0[dx+3]=255;
          }
        }
      break;
      }
    case TextureFormat::RG16: {
      for(uint32_t iy=0;iy<sh;++iy){
        auto data0=data+((y+iy)*dw+dx);
        auto src0 =reinterpret_cast<const uint16_t*>(src+iy*sw);
        for(uint32_t ix=0,dx=0;ix<sw;dx+=4,ix+=3){
          data0[dx  ]=src0[ix  ]/256;
          data0[dx+1]=src0[ix  ]/256;
          data0[dx+2]=src0[ix  ]/256;
          data0[dx+3]=src0[ix+1]/256;
          }
        }
      break;
      }
    case TextureFormat::R16: {
      for(uint32_t iy=0;iy<sh;++iy){
        auto data0=data+((y+iy)*dw+dx);
        auto src0 =reinterpret_cast<const uint16_t*>(src+iy*sw);
        for(uint32_t ix=0,dx=0;ix<sw;dx+=4,ix++){
          data0[dx  ]=255;
          data0[dx+1]=255;
          data0[dx+2]=255;
          data0[dx+3]=src0[ix]/256;
          }
        }
      break;
      }
    case TextureFormat::RGBA8: {
      for(uint32_t iy=0;iy<sh;++iy)
        std::memcpy(data+((y+iy)*dw+dx),src+iy*sw,sw);
      break;
      }
    case TextureFormat::RGB8: {
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
    case TextureFormat::RG8: {
      for(uint32_t iy=0;iy<sh;++iy){
        auto data0=data+((y+iy)*dw+dx);
        auto src0 =src+iy*sw;
        for(uint32_t ix=0,dx=0;ix<sw;dx+=4,ix+=2){
          data0[dx  ]=src0[ix  ];
          data0[dx+1]=src0[ix  ];
          data0[dx+2]=src0[ix  ];
          data0[dx+3]=src0[ix+1];
          }
        }
      break;
      }
    case TextureFormat::R8: {
      for(uint32_t iy=0;iy<sh;++iy){
        auto data0=data+((y+iy)*dw+dx);
        auto src0 =src+iy*sw;
        for(uint32_t ix=0,dx=0;ix<sw;dx+=4,ix++){
          data0[dx  ]=255;
          data0[dx+1]=255;
          data0[dx+2]=255;
          data0[dx+3]=src0[ix];
          }
        }
      break;
      }
    }

  //cpu.save("dbg.png");
  }
