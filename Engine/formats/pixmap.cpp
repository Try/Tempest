#include "pixmap.h"

#include <Tempest/File>
#include <Tempest/Except>

#include <vector>
#include <cstring>

#include <squish.h>

#include "thirdparty/stb_image.h"
#include "thirdparty/stb_image_write.h"

#include "ddsdef.h"

using namespace Tempest;

struct Pixmap::Impl {
  Impl()=default;

  static uint32_t bppFromFrm(Pixmap::Format frm) {
    switch(frm) {
      case Pixmap::Format::A:    return 1;
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
      if(obpp==3){
        // RGB -> RGBA
        for(size_t i=0;i<size;++i){
          uint32_t&       pix=reinterpret_cast<uint32_t*>(data)[i];
          const uint8_t*  s  =other.data+i*3;
          pix = uint32_t(s[0])<<0 | uint32_t(s[1])<<8 | uint32_t(s[2])<<16 | uint32_t(255)<<24;
          }
        }
      else if(obpp==1){
        // A -> RGBA
        for(size_t i=0;i<size;++i){
          uint32_t&       pix=reinterpret_cast<uint32_t*>(data)[i];
          const uint8_t*  s  =other.data+i;
          pix = uint32_t(255)<<0 | uint32_t(255)<<8 | uint32_t(255)<<16 | uint32_t(s[0])<<24;
          }
        }
      return;
      }

    //TODO
    throw std::runtime_error("unimplemented");
    }

  Impl(IDevice& f){
    int iw=0,ih=0,channels=0;
    data = load(f,iw,ih,channels,STBI_default);

    if(data==nullptr && channels==0)
      throw std::system_error(Tempest::SystemErrc::UnableToLoadAsset);

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

  static bool isDDS(const stbi__context& s) {
    return s.buflen>=4 && std::memcmp(reinterpret_cast<const char*>(s.buffer_start),"DDS ",4)==0;
    }

  static stbi_uc* load(IDevice& f,int& x,int& y,int& comp,int req_comp){
    unsigned char *result;
    stbi__context s;
    stbi__start_file(&s,&f);

    if(isDDS(s))
      return loadDDS(&s,x,y,comp);

    result = stbi__load_and_postprocess_8bit(&s,&x,&y,&comp,req_comp);
    if( result ){
      // need to 'unget' all the characters in the IO buffer
      // fseek(f, - (int) (s.img_buffer_end - s.img_buffer), SEEK_CUR);
      }
    return result;
    }

  static stbi_uc* loadDDS(stbi__context* s,int& x,int& y,int& bpp){
    using namespace Tempest::Detail;

    stbi_uc head[4]={};
    stbi__getn(s,head,4);

    DDSURFACEDESC2 ddsd;
    if(stbi__getn(s,reinterpret_cast<stbi_uc*>(&ddsd),sizeof(ddsd))==0)
      return nullptr;
    x = int(ddsd.dwWidth);
    y = int(ddsd.dwHeight);

    int compressType = squish::kDxt1;
    switch( ddsd.ddpfPixelFormat.dwFourCC ) {
      case FOURCC_DXT1:
        bpp    = 3;
        compressType = squish::kDxt1;
        break;

      case FOURCC_DXT3:
        bpp    = 4;
        compressType = squish::kDxt3;
        break;

      case FOURCC_DXT5:
        bpp    = 4;
        compressType = squish::kDxt5;
        break;

      default:
        return nullptr;
      }

    if( ddsd.dwLinearSize == 0 ) {
      //return false;
      }

    size_t blockcount = size_t(((x+3)/4)*((y+3)/4));
    size_t blocksize  = (compressType==squish::kDxt1) ? 8 : 16;
    size_t bufferSize = blockcount*blocksize;

    std::unique_ptr<stbi_uc[]> ddsv(new(std::nothrow) stbi_uc[bufferSize]);
    if(!ddsv || stbi__getn(s,ddsv.get(),int(bufferSize))!=1 )
      return nullptr;

    bpp = 4;
    return ddsToRgba(ddsv.get(),uint32_t(x),uint32_t(y),compressType);
    }

  static stbi_uc* ddsToRgba(const stbi_uc* dds,const uint32_t w,const uint32_t h,const int frm){
    squish::u8 pixels[4][4][4];

    stbi_uc*       px        = reinterpret_cast<stbi_uc*>(STBI_MALLOC(w*h*4));
    const uint32_t bpp       = 4;
    const uint32_t w4        = (w+3)/4;
    const uint32_t blocksize = (frm==squish::kDxt1) ? 8 : 16;

    if(px==nullptr)
      return nullptr;

    for(uint32_t i=0; i<w; i+=4)
      for(uint32_t r=0; r<h; r+=4){
        uint32_t pos = ((i/4) + (r/4)*w4)*blocksize;
        squish::Decompress( &pixels[0][0][0], &dds[pos], frm );

        for(uint32_t x=0; x<4; ++x)
          for(uint32_t y=0; y<4; ++y){
            uint8_t * v = &px[ (i+x + (r+y)*w)*bpp ];
            std::memcpy( v, pixels[y][x], bpp);
            }
        }
    return px;
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

Pixmap::Pixmap(IDevice &input) {
  impl.reset(new Impl(input));
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
