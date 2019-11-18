#include "pixmap.h"

#include <Tempest/File>
#include <Tempest/Except>

#include "pixmapcodec.h"

#include <vector>
#include <cstring>
#include <squish.h>

using namespace Tempest;

struct Pixmap::Impl {
  Impl()=default;

  static uint32_t bppFromFrm(Pixmap::Format frm) {
    switch(frm) {
      case Pixmap::Format::A:    return 1;
      case Pixmap::Format::RGB:  return 3;
      case Pixmap::Format::RGBA: return 4;
      case Pixmap::Format::DXT1: return 0;
      case Pixmap::Format::DXT3: return 0;
      case Pixmap::Format::DXT5: return 0;
      }
    return 1;
    }

  Impl(uint32_t w,uint32_t h,Pixmap::Format frm):w(w),h(h),frm(frm) {
    bpp    = bppFromFrm(frm);
    dataSz = size_t(w)*size_t(h)*size_t(bpp);
    data   = reinterpret_cast<uint8_t*>(std::malloc(dataSz));
    if(!data)
      throw std::bad_alloc();
    std::memset(data,0,dataSz);
    }

  Impl(const Impl& other):w(other.w),h(other.h),bpp(other.bpp),dataSz(other.dataSz),frm(other.frm),mipCnt(other.mipCnt){
    size_t size=size_t(w)*size_t(h)*size_t(bpp);
    data=reinterpret_cast<uint8_t*>(std::malloc(size));
    if(!data)
      throw std::bad_alloc();

    memcpy(data,other.data,size);
    }

  Impl(const Impl& other,Pixmap::Format conv):w(other.w),h(other.h),bpp(other.bpp),frm(conv) {
    bpp = bppFromFrm(frm);

    size_t size=size_t(w)*size_t(h)*size_t(bpp);
    data=reinterpret_cast<uint8_t*>(std::malloc(size));
    if(!data)
      throw std::bad_alloc();

    dataSz = size;
    if(bpp==other.bpp) {
      memcpy(data,other.data,size);
      return;
      }

    if(frm==Format::RGBA) {
      const size_t size=size_t(w)*size_t(h);
      if(other.frm==Format::RGB){
        // RGB -> RGBA
        for(size_t i=0;i<size;++i){
          uint32_t&       pix=reinterpret_cast<uint32_t*>(data)[i];
          const uint8_t*  s  =other.data+i*3;
          pix = uint32_t(s[0])<<0 | uint32_t(s[1])<<8 | uint32_t(s[2])<<16 | uint32_t(255)<<24;
          }
        }
      else if(other.frm==Format::A){
        // A -> RGBA
        for(size_t i=0;i<size;++i){
          uint32_t&       pix=reinterpret_cast<uint32_t*>(data)[i];
          const uint8_t*  s  =other.data+i;
          pix = uint32_t(255)<<0 | uint32_t(255)<<8 | uint32_t(255)<<16 | uint32_t(s[0])<<24;
          }
        }
      else if(other.frm==Format::DXT1){
        // dxt1 -> RGBA
        ddsToRgba(data,other.data,uint32_t(w),uint32_t(h),squish::kDxt1);
        }
      else if(other.frm==Format::DXT3){
        // dxt3 -> RGBA
        ddsToRgba(data,other.data,uint32_t(w),uint32_t(h),squish::kDxt3);
        }
      else if(other.frm==Format::DXT5){
        // dxt5 -> RGBA
        ddsToRgba(data,other.data,uint32_t(w),uint32_t(h),squish::kDxt5);
        }
      return;
      }

    //TODO
    throw std::runtime_error("unimplemented");
    }

  Impl(IDevice& f){
    frm  = Pixmap::Format::RGBA;
    data = PixmapCodec::loadImg(f,w,h,frm,mipCnt,bpp,dataSz);

    if(data==nullptr && bpp==0)
      throw std::system_error(Tempest::SystemErrc::UnableToLoadAsset);
    }

  ~Impl(){
    PixmapCodec::freeImg(data);
    }

  void save(ODevice& f,const char* ext){
    PixmapCodec::saveImg(f,ext,data,dataSz,w,h,frm);
    }

  static void ddsToRgba(uint8_t* px,const uint8_t* dds,const uint32_t w,const uint32_t h,const int frm) {
    squish::u8 pixels[4][4][4];

    const uint32_t bpp       = 4;
    const uint32_t w4        = (w+3)/4;
    const uint32_t blocksize = (frm==squish::kDxt1) ? 8 : 16;

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
    }

  uint8_t*       data   = nullptr;
  uint32_t       w      = 0;
  uint32_t       h      = 0;
  uint32_t       bpp    = 0;
  size_t         dataSz = 0;
  Pixmap::Format frm    = Pixmap::Format::RGB;
  uint32_t       mipCnt = 1;

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

void Pixmap::save(const char *path, const char *ext) const {
  WFile f(path);
  save(f,ext);
  }

void Pixmap::save(ODevice &f, const char *ext) const {
  impl->save(f,ext);
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

uint32_t Pixmap::mipCount() const {
  return impl->mipCnt;
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

size_t Pixmap::dataSize() const {
  return impl->dataSz;
  }

Pixmap::Format Pixmap::format() const {
  return impl->frm;
  }
