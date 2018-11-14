#include "pixmap.h"

#include <Tempest/File>
#include "thirdparty/stb_image.h"

using namespace Tempest;

struct Pixmap::Impl {
  Impl(RFile& f){
    int iw=0,ih=0,channels=0;
    data = load(f,iw,ih,channels,STBI_rgb_alpha);
    w    = uint32_t(iw);
    h    = uint32_t(ih);
    bpp  = uint32_t(channels);
    }

  ~Impl(){
    stbi_image_free(data);
    }

  static int stbiRead(void *user, char *data, int size) {
    return int(reinterpret_cast<IDevice*>(user)->read(data,size_t(size)));
    }

  static void stbiSkip(void *user, int n) {
    reinterpret_cast<IDevice*>(user)->seek(size_t(n));
    }

  static int stbiEof(void *user) {
    return false;
    //return reinterpret_cast<IDevice*>(user)->peek();
    }

  static void stbi__start_file(stbi__context *s, IDevice *f) {
    static stbi_io_callbacks stbi__stdio_callbacks = {
      stbiRead,
      stbiSkip,
      stbiEof,
      };

    stbi__start_callbacks(s,&stbi__stdio_callbacks,reinterpret_cast<void*>(f));
    }

  static stbi_uc* load(IDevice& f,int& x,int& y,int& comp,int req_comp){
    unsigned char *result;
    stbi__context s;
    stbi__start_file(&s,&f);
    result = stbi__load_and_postprocess_8bit(&s,&x,&y,&comp,req_comp);
    if( result ){
      // need to 'unget' all the characters in the IO buffer
      // fseek(f, - (int) (s.img_buffer_end - s.img_buffer), SEEK_CUR);
      }
    return result;

    //return stbi_load_from_file(f,&w,&h,&channels,req_comp);
    }

  stbi_uc* data = nullptr;
  uint32_t w    = 0;
  uint32_t h    = 0;
  size_t   bpp  = 0;
  };

Pixmap::Pixmap(const char* path) {
  RFile f(path);
  impl.reset(new Impl(f));
  }

Pixmap::Pixmap(const std::string &path) {
  RFile f(path);
  impl.reset(new Impl(f));
  }

Pixmap::Pixmap(const char16_t *path) {
  RFile f(path);
  impl.reset(new Impl(f));
  }

Pixmap::Pixmap(const std::u16string &path) {
  RFile f(path);
  impl.reset(new Impl(f));
  }

Pixmap::Pixmap(Pixmap &&p)
  :impl(std::move(p.impl)){
  }

Pixmap& Pixmap::operator=(Pixmap &&p) {
  impl=std::move(p.impl);
  return *this;
  }

Pixmap::~Pixmap() {
  }

uint32_t Pixmap::w() const {
  return impl->w;
  }

uint32_t Pixmap::h() const {
  return impl->h;
  }

bool Pixmap::isEmpty() const {
  return impl->w<=0 || impl->h<=0;
  }

const void *Pixmap::data() const {
  return impl->data;
  }
