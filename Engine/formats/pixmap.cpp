#include "pixmap.h"

#include <Tempest/File>
#include "thirdparty/stb_image.h"
#include "thirdparty/stb_image_write.h"

using namespace Tempest;

struct Pixmap::Impl {
  Impl()=default;

  static uint32_t bppFromFrm(Pixmap::Format frm) {
    switch(frm) {
      case Pixmap::Format::RGB:  return 3;
      case Pixmap::Format::RGBA: return 4;
      }
    return 1;
    }

  Impl(uint32_t w,uint32_t h,Pixmap::Format frm):w(w),h(h),frm(frm) {
    bpp = bppFromFrm(frm);

    uint32_t size=w*h*bpp;
    data=reinterpret_cast<stbi_uc*>(STBI_MALLOC(size));
    if(!data)
      throw std::bad_alloc();
    memset(data,0,size);
    }

  Impl(const Impl& other):w(other.w),h(other.h),bpp(other.bpp),frm(other.frm){
    size_t size=w*h*bpp;
    data=reinterpret_cast<stbi_uc*>(STBI_MALLOC(size));
    if(!data)
      throw std::bad_alloc();

    memcpy(data,other.data,size);
    }

  Impl(const Impl& other,Pixmap::Format conv):w(other.w),h(other.h),bpp(other.bpp),frm(conv) {
    bpp = bppFromFrm(frm);

    size_t size=w*h*bpp;
    data=reinterpret_cast<stbi_uc*>(STBI_MALLOC(size));
    if(!data)
      throw std::bad_alloc();

    if(bpp==other.bpp) {
      memcpy(data,other.data,size);
      return;
      }

    if(bpp==4) {
      const size_t obpp=other.bpp;
      const size_t size=w*h;
      for(size_t i=0;i<size;++i){
        uint32_t&       pix=reinterpret_cast<uint32_t*>(data)[i];
        const uint8_t*  s  =other.data+i*obpp;
        pix = uint32_t(s[0])<<0 | uint32_t(s[1])<<8 | uint32_t(s[2])<<16 | uint32_t(255)<<24;
        }
      return;
      }
    }

  Impl(RFile& f){
    int iw=0,ih=0,channels=0;
    data = load(f,iw,ih,channels,STBI_default);
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

Pixmap::Pixmap(const Pixmap &src, Pixmap::Format conv)
  :impl(new Impl(*src.impl,conv)){
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

Pixmap::Pixmap(const Pixmap &src)
  :impl(new Impl(*src.impl)){
  }

Pixmap::Pixmap(Pixmap &&p)
  :impl(std::move(p.impl)){
  }

Pixmap& Pixmap::operator=(Pixmap &&p) {
  impl=std::move(p.impl);
  return *this;
  }

Pixmap& Pixmap::operator=(const Pixmap &p) {
  if(p.impl.get()==&Impl::zero)
    impl.reset(&Impl::zero); else
    impl.reset(new Impl(*p.impl));
  return *this;
  }

Pixmap::~Pixmap() {
  }

bool Pixmap::save(const char *path, const char *ext) {
  try {
    WFile f(path);
    if(ext==nullptr)
      ext="PNG";
    return impl->save(f,ext);
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

uint32_t Pixmap::bpp() const {
  return uint32_t(impl->bpp);
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
