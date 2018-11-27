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

static uint32_t bytePerPix(const Pixmap& p){
  switch(p.format()){
    case Pixmap::Format::RGB:  return 3;
    case Pixmap::Format::RGBA: return 4;
    }
  return 1;
  }

Sprite TextureAtlas::emplace(TextureAtlas::Page &dest, const Pixmap &p, uint32_t x, uint32_t y) {
  auto     data=reinterpret_cast<uint8_t*>(dest.cpu.data());
  uint32_t dbpp=bytePerPix(dest.cpu);
  uint32_t dx  =x*dbpp;
  uint32_t dw  =p.w()*dbpp;

  auto     src =reinterpret_cast<const uint8_t*>(p.data());
  uint32_t sbpp=bytePerPix(p);
  uint32_t sw  =p.w()*sbpp;
  uint32_t sh  =p.h();//*sbpp;

  if(dest.cpu.format()==p.format()){
    for(uint32_t iy=0;iy<sh;++iy)
      memcpy(data+((y+iy)*dw+dx),src+iy*sw,sw);
    } else {
    throw "TODO";
    }

  dest.cpu.save("dbg.png");
  return Sprite(&dest,x,y,p.w(),p.h());
  }
