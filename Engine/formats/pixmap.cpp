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

  Impl(uint32_t w,uint32_t h,Pixmap::Format frm):w(w),h(h),frm(frm) {
    bpp    = uint32_t(bppForFormat(frm));
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

    std::memcpy(data,other.data,size);
    }

  Impl(const Impl& other,Pixmap::Format conv):w(other.w),h(other.h),bpp(other.bpp),frm(conv) {
    bpp = uint32_t(bppForFormat(frm));

    size_t size = size_t(w)*size_t(h)*size_t(bpp);
    data = reinterpret_cast<uint8_t*>(std::malloc(size));
    if(!data)
      throw std::bad_alloc();
    dataSz = size;

    if(frm==Format::RGBA && other.frm==Format::RGB) {
      // specialize a common case
      const size_t sz = size_t(w)*size_t(h);
      for(size_t i=0;i<sz;++i) {
        uint32_t&       pix=reinterpret_cast<uint32_t*>(data)[i];
        const uint8_t*  s  =other.data+i*3;
        pix = uint32_t(s[0])<<0 | uint32_t(s[1])<<8 | uint32_t(s[2])<<16 | uint32_t(255)<<24;
        }
      return;
      }

    if(isCompressed(other.frm)) {
      if(frm!=Format::RGB && frm!=Format::RGBA)
        throw std::runtime_error("assert"); // handle outside of this function
      static const int kfrm[] = {squish::kDxt1,squish::kDxt3,squish::kDxt5};
      if(frm==Format::RGB)
        ddsToRgba(data,other.data,w,h,kfrm[uint8_t(other.frm)-uint8_t(Format::DXT1)],3); else
        ddsToRgba(data,other.data,w,h,kfrm[uint8_t(other.frm)-uint8_t(Format::DXT1)],4);
      return;
      }

    if(isCompressed(frm)) {
      // TODO: image compression
      throw std::runtime_error("unimplemented");
      }

    // noncompressed
    const uint8_t compDst = componentsCount(frm);
    const uint8_t compSrc = componentsCount(other.frm);

    const uint8_t byteDst = bytesPerChannel(frm);
    const uint8_t byteSrc = bytesPerChannel(other.frm);

    switch(byteDst) {
      case 1:{
        switch(byteSrc) {
          case 1: noncompresedConv<uint8_t,uint8_t> (w,h, data,other.data, compDst, compSrc); return;
          case 2: noncompresedConv<uint8_t,uint16_t>(w,h, data,other.data, compDst, compSrc); return;
          }
        }
      case 2:{
        switch(byteSrc) {
          case 1: noncompresedConv<uint16_t,uint8_t> (w,h, data,other.data, compDst, compSrc); return;
          case 2: noncompresedConv<uint16_t,uint16_t>(w,h, data,other.data, compDst, compSrc); return;
          }
        }
      }

    // unreachable
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

  static std::unique_ptr<Impl,Deleter> convert(const Impl& other,Format frm) {
    if(other.frm==frm)
      return std::unique_ptr<Impl,Deleter>(new Impl(other)); //copy

    if(isCompressed(other.frm)) {
      if(frm!=Format::RGB && frm!=Format::RGBA) {
        // cross-conversion: DDS -> RGBA -> frm
        Impl tmp(other,Format::RGBA);
        return std::unique_ptr<Impl,Deleter>(new Impl(tmp,frm));
        }
      }

    return std::unique_ptr<Impl,Deleter>(new Impl(other,frm));
    }

  template<class Tout, class Tin>
  static void noncompresedConv(size_t w, size_t h, void* vdata, const void* vsrc, uint8_t eltOut, uint8_t eltIn){
    auto* data = reinterpret_cast<Tout*>(vdata);
    auto* src  = reinterpret_cast<const Tin*>(vsrc);

    const size_t size = w*h;
    for(size_t i=0;i<size;++i) {
      Tout*       pix = data+i*eltOut;
      const Tin*  s   = src +i*eltIn;

      Tout tmp[4] = {0,0,0,Tout(-1)};
      for(uint8_t i=0;i<eltIn;++i)
        copy(tmp[i],s[i]);

      for(uint8_t i=0;i<eltOut;++i)
        copy(pix[i],tmp[i]);
      }
    }

  static void copy(uint8_t& r,uint8_t v){
    r = v;
    }
  static void copy(uint16_t& r,uint16_t v){
    r = v;
    }
  static void copy(uint8_t& r,uint16_t v){
    r = v/256;
    }
  static void copy(uint16_t& r,uint8_t v){
    r = v*256+255*(v%2);
    }

  static uint8_t componentsCount(Pixmap::Format frm) {
    switch(frm) {
      case Pixmap::Format::R:      return 1;
      case Pixmap::Format::RG:     return 2;
      case Pixmap::Format::RGB:    return 3;
      case Pixmap::Format::RGBA:   return 4;
      //---
      case Pixmap::Format::R16:    return 1;
      case Pixmap::Format::RG16:   return 2;
      case Pixmap::Format::RGB16:  return 3;
      case Pixmap::Format::RGBA16: return 4;
      //---
      case Pixmap::Format::DXT1:   return 0;
      case Pixmap::Format::DXT3:   return 0;
      case Pixmap::Format::DXT5:   return 0;
      }
    return 0;
    }

  static uint8_t bytesPerChannel(Pixmap::Format frm) {
    switch(frm) {
      case Pixmap::Format::R:      return 1;
      case Pixmap::Format::RG:     return 1;
      case Pixmap::Format::RGB:    return 1;
      case Pixmap::Format::RGBA:   return 1;
      //---
      case Pixmap::Format::R16:    return 2;
      case Pixmap::Format::RG16:   return 2;
      case Pixmap::Format::RGB16:  return 2;
      case Pixmap::Format::RGBA16: return 2;
      //---
      case Pixmap::Format::DXT1:   return 0;
      case Pixmap::Format::DXT3:   return 0;
      case Pixmap::Format::DXT5:   return 0;
      }
    return 0;
    }

  static bool isCompressed(Pixmap::Format frm) {
    return frm==Pixmap::Format::DXT1 ||
           frm==Pixmap::Format::DXT3 ||
           frm==Pixmap::Format::DXT5;
    }

  void save(ODevice& f,const char* ext){
    PixmapCodec::saveImg(f,ext,data,dataSz,w,h,frm);
    }

  static void ddsToRgba(uint8_t* px,const uint8_t* dds,const uint32_t w,const uint32_t h,const int frm,uint8_t bpp) {
    squish::u8 pixels[4][4][4];

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
  :impl(Impl::convert(*src.impl,conv)){
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

size_t Pixmap::bppForFormat(Pixmap::Format frm) {
  switch(frm) {
    case Pixmap::Format::R:      return 1;
    case Pixmap::Format::RG:     return 2;
    case Pixmap::Format::RGB:    return 3;
    case Pixmap::Format::RGBA:   return 4;
    //---
    case Pixmap::Format::R16:    return 2;
    case Pixmap::Format::RG16:   return 4;
    case Pixmap::Format::RGB16:  return 6;
    case Pixmap::Format::RGBA16: return 8;
    //---
    case Pixmap::Format::DXT1:   return 0;
    case Pixmap::Format::DXT3:   return 0;
    case Pixmap::Format::DXT5:   return 0;
    }
  return 0;
  }
