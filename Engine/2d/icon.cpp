#include "icon.h"

using namespace Tempest;

Icon::Icon() {
  }

Icon::Icon(const Pixmap &pm, TextureAtlas &atl) {
  Sprite norm = atl.load(pm);
  set(ST_Normal,norm);

  const uint32_t w = pm.w(), h = pm.h();

  Pixmap dst = Pixmap(w,h,pm.format());
  const uint8_t* p = reinterpret_cast<const uint8_t*>(pm.data());
  uint8_t*       d = reinterpret_cast<uint8_t*>(dst.data());

  switch(pm.format()) {
    case TextureFormat::RGB8: {
      for(uint32_t r=0; r<h; ++r)
        for(uint32_t i=0; i<w; ++i){
          //0.299, 0.587, 0.114
          uint8_t cl = uint8_t(p[0]*0.299 + p[1]*0.587 + p[2]*0.114);
          d[0] = cl;
          d[1] = cl;
          d[2] = cl;

          p += 3;
          d += 3;
          }
      set(ST_Disabled,atl.load(dst));
      break;
      }
    case TextureFormat::RGBA8: {
      for(uint32_t r=0; r<h; ++r)
        for(uint32_t i=0; i<w; ++i){
          //0.299, 0.587, 0.114
          uint8_t cl = uint8_t(p[0]*0.299 + p[1]*0.587 + p[2]*0.114);
          d[0] = cl;
          d[1] = cl;
          d[2] = cl;
          d[3] = p[3];

          p += 4;
          d += 4;
          }
      set(ST_Disabled,atl.load(dst));
      break;
      }
    default: {
      set(ST_Disabled,norm);
     }
    }
  }

const Sprite &Icon::sprite(int w,int h,Icon::State st) const {
  return val[st].sprite(w,h);
  }

void Icon::set(Icon::State st,const Sprite &s) {
  return val[st].set(s);
  }

const Sprite& Icon::SzArray::sprite(int w, int h) const {
  if(emplace.w()==w && emplace.h()==h)
    return emplace;

  const Sprite* ret=data.size() ? &data.back() : &emplace;

  if( emplace.w()<=w && emplace.h()<=h )
    if( ret->w()<emplace.w() || (ret->w()==emplace.w() && ret->h()<emplace.h()) )
      ret=&emplace;

  for(auto& i:data)
    if( i.w()<=w && i.h()<=h )
      if( ret->w()<i.w() || (ret->w()==i.w() && ret->h()<i.h()) )
        ret=&i;
  if( ret->w()<=w && ret->h()<=h )
    return *ret;

  return emplace;
  }

void Icon::SzArray::set(const Sprite &s) {
  if( emplace.size().isEmpty() || emplace.size()==s.size() ) {
    emplace=s;
    return;
    }

  for(auto& i:data)
    if(i.size()==s.size()){
      i=s;
      return;
      }
  data.emplace_back(s);
  }
