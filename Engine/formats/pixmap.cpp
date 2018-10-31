#include "pixmap.h"

#include "thirdparty/stb_image.h"

using namespace Tempest;

struct Pixmap::Impl {
  Impl(const char* path){
    int iw=0,ih=0,channels=0;
    data = stbi_load(path,&iw,&ih,&channels,STBI_rgb_alpha);
    w    = uint32_t(iw);
    h    = uint32_t(ih);
    bpp  = uint32_t(channels);
    }

  ~Impl(){
    stbi_image_free(data);
    }

  stbi_uc* data = nullptr;
  uint32_t w    = 0;
  uint32_t h    = 0;
  size_t   bpp  = 0;
  };

Pixmap::Pixmap(const char* path) {
  impl.reset(new Impl(path));
  }

Pixmap::~Pixmap() {
  }

uint32_t Pixmap::w() const {
  return impl->w;
  }

uint32_t Pixmap::h() const {
  return impl->h;
  }

const void *Pixmap::data() const {
  return impl->data;
  }
