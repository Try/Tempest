#include "font.h"

#include "thirdparty/stb_truetype.h"

using namespace Tempest;

struct FontElement::Impl {
  Impl(const char *filename){
    if(filename==nullptr)
      return;

    RFile file(filename);
    size = file.size();
    data = new uint8_t[size];

    if(file.read(data,size)!=size){
      delete data;
      throw std::bad_alloc(); //TODO
      }
    }

  ~Impl(){
    delete data;
    }

  uint8_t*       data=nullptr;
  size_t         size=0;
  stbtt_fontinfo info={};
  };

FontElement::FontElement(const char *file)
  :ptr(std::make_shared<Impl>(file)) {
  stbtt_InitFont(&ptr->info,ptr->data,0);
  }

Font::Font(const char *file)
  : fnt{{file,nullptr},{nullptr,nullptr}}{
  fnt[1][0]=fnt[0][0];
  fnt[1][1]=fnt[0][0];
  fnt[0][1]=fnt[0][0];
  }
