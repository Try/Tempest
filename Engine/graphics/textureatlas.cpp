#include "textureatlas.h"

#include <Tempest/Sprite>

using namespace Tempest;

TextureAtlas::TextureAtlas(Device& device)
  :device(device),root(defPgSize,defPgSize){
  root.useCount.fetch_add(1);
  }

TextureAtlas::~TextureAtlas() {
  Page* pg=root.next;
  while(pg) {
    auto del=pg;
    pg=pg->next;
    delete del;
    }
  }

Sprite TextureAtlas::load(const Pixmap &pm) {
  Page*  pg=&root;
  Sprite ret;

  pg->addRef();
  while( true ) {
    ret = tryLoad(*pg,pm);
    if(!ret.isEmpty())
      return ret;

    {
      std::lock_guard<std::mutex> guard(sync);
      if(pg->next){
        auto next=pg->next;
        pg->decRef();
        pg=next;
        pg->addRef();
        continue;
        }
      pg->next=new Page(defPgSize,defPgSize);
      auto next=pg->next;
      pg->decRef();
      pg=next;
      pg->addRef();
    }

    ret = tryLoad(*pg,pm);
    if(!ret.isEmpty())
      return ret;
    pg->decRef();
    throw std::bad_alloc();
    }
  }

Sprite TextureAtlas::tryLoad(TextureAtlas::Page &dest,const Pixmap &p) {
  std::lock_guard<std::mutex> guard(dest.sync);

  int pw=int(p.w());
  int ph=int(p.h());

  for(size_t i=0;i<dest.rect.size();++i){
    if(pw<dest.rect[i].w && ph<dest.rect[i].h){
      Rect rd=dest.rect[i];
      dest.rect[i]=Rect(rd.x+pw,rd.y+ph,rd.w-pw,rd.h-ph);
      dest.rect.emplace_back(rd.x+pw,rd.y,rd.w-pw,ph);
      dest.rect.emplace_back(rd.x,rd.y+ph,pw,rd.h-ph);
      return emplace(dest,p,uint32_t(rd.x),uint32_t(rd.y));
      }
    if(pw==dest.rect[i].w && ph<dest.rect[i].h){
      Rect rd=dest.rect[i];
      dest.rect[i]=Rect(rd.x+pw,rd.y+ph,rd.w-pw,rd.h-ph);
      //dest.rect.emplace_back(rd.x+pw,rd.y,rd.w-pw,ph);
      dest.rect.emplace_back(rd.x,rd.y+ph,pw,rd.h-ph);
      return emplace(dest,p,uint32_t(rd.x),uint32_t(rd.y));
      }
    if(pw<dest.rect[i].w && ph==dest.rect[i].h){
      Rect rd=dest.rect[i];
      dest.rect[i]=Rect(rd.x+pw,rd.y+ph,rd.w-pw,rd.h-ph);
      dest.rect.emplace_back(rd.x+pw,rd.y,rd.w-pw,ph);
      //dest.rect.emplace_back(rd.x,rd.y+ph,pw,rd.h-ph);
      return emplace(dest,p,uint32_t(rd.x),uint32_t(rd.y));
      }
    if(pw==dest.rect[i].w && ph==dest.rect[i].h){
      Point rd=dest.rect[i].pos();
      dest.rect[i]=dest.rect.back();
      dest.rect.pop_back();
      return emplace(dest,p,uint32_t(rd.x),uint32_t(rd.y));
      }
    }
  return Sprite();
  }

Sprite TextureAtlas::emplace(TextureAtlas::Page &dest, const Pixmap &p, uint32_t x, uint32_t y) {
  auto     data=reinterpret_cast<uint8_t*>(dest.cpu.data());
  //uint32_t dbpp=4;//dest.cpu.bpp();
  uint32_t dx  =x*4;
  uint32_t dw  =dest.cpu.w()*4;

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

  // dest.cpu.save("dbg.png");
  return Sprite(&dest,x,y,p.w(),p.h());
  }
