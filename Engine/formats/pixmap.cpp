#include "pixmap.h"

#include <Tempest/File>
#include "thirdparty/stb_image.h"
#include "thirdparty/stb_image_write.h"

using namespace Tempest;

struct Pixmap::Impl {
  Impl()=default;

  Impl(uint32_t w,uint32_t h,Pixmap::Format frm):w(w),h(h),frm(frm){
    switch(frm) {
      case Pixmap::Format::RGB:  bpp=3; break;
      case Pixmap::Format::RGBA: bpp=4; break;
      }

    uint32_t size=w*h*bpp;
    data=reinterpret_cast<stbi_uc*>(STBI_MALLOC(size));
    if(!data)
      throw std::bad_alloc();
    memset(data,0,size);
    }

  Impl(RFile& f){
    int iw=0,ih=0,channels=0;
    data = load(f,iw,ih,channels,STBI_rgb_alpha);
    w    = uint32_t(iw);
    h    = uint32_t(ih);
    bpp  = uint32_t(channels);
    if(bpp==3)
      frm=Pixmap::Format::RGB; else
      frm=Pixmap::Format::RGBA;
    }

  ~Impl(){
    stbi_image_free(data);
    }

  //TODO: ext
  bool save(WFile& f,const char* /*ext*/){
    int len=0;
    int stride_bytes=0;

    unsigned char *png = stbi_write_png_to_mem(data,stride_bytes,int(w),int(h),int(bpp),&len);
    if(png==nullptr)
      return false;

    bool ret=(f.write(png,size_t(len))==size_t(len) && f.flush());
    STBIW_FREE(png);
    return ret;
    }

  static int stbiRead(void *user, char *data, int size) {
    return int(reinterpret_cast<IDevice*>(user)->read(data,size_t(size)));
    }

  static void stbiSkip(void *user, int n) {
    reinterpret_cast<IDevice*>(user)->seek(size_t(n));
    }

  static int stbiEof(void */*user*/) {
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
    }

  stbi_uc*       data = nullptr;
  uint32_t       w    = 0;
  uint32_t       h    = 0;
  size_t         bpp  = 0;
  Pixmap::Format frm  = Pixmap::Format::RGB;

  static Impl zero;
  };

Pixmap::Impl Pixmap::Impl::zero;

void Pixmap::Deleter::operator()(Pixmap::Impl *ptr) {
  if(ptr!=&Pixmap::Impl::zero)
    delete ptr;
  }

Pixmap::Pixmap():impl(&Impl::zero){
  }

Pixmap::Pixmap(uint32_t w, uint32_t h, Pixmap::Format frm)
  :impl(new Impl(w,h,frm)){

  }

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

bool Pixmap::save(const char *path, const char *ext) {
  try {
    WFile f(path);
    if(ext==nullptr)
      ext="PNG";
    impl->save(f,ext);
    }
  catch(...) {
    return false;
    }
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

void *Pixmap::data() {
  return impl->data;
  }

Pixmap::Format Pixmap::format() const {
  return impl->frm;
  }
