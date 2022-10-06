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
    dataSz = calcDataSize(w,h,frm);
    data   = reinterpret_cast<uint8_t*>(std::malloc(dataSz));
    if(!data)
      throw std::bad_alloc();
    std::memset(data,0,dataSz);
    }

  Impl(const Impl& other):w(other.w),h(other.h),dataSz(other.dataSz),frm(other.frm),mipCnt(other.mipCnt){
    data   = reinterpret_cast<uint8_t*>(std::malloc(dataSz));
    if(!data)
      throw std::bad_alloc();
    std::memcpy(data,other.data,dataSz);
    }

  Impl(const Impl& other,Pixmap::Format conv):w(other.w),h(other.h),frm(conv) {
    size_t size = calcDataSize(w,h,frm);
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
    const uint8_t compDst = Pixmap::componentCount(frm);
    const uint8_t compSrc = Pixmap::componentCount(other.frm);

    const uint8_t byteDst = bytesPerChannel(frm);
    const uint8_t byteSrc = bytesPerChannel(other.frm);

    switch(byteDst) {
      case 1:{
        switch(byteSrc) {
          case 1: noncompresedConv<uint8_t,uint8_t> (w,h, data,other.data, compDst, compSrc); return;
          case 2: noncompresedConv<uint8_t,uint16_t>(w,h, data,other.data, compDst, compSrc); return;
          case 4: noncompresedConv<uint8_t,float>   (w,h, data,other.data, compDst, compSrc); return;
          }
        }
      case 2:{
        switch(byteSrc) {
          case 1: noncompresedConv<uint16_t,uint8_t> (w,h, data,other.data, compDst, compSrc); return;
          case 2: noncompresedConv<uint16_t,uint16_t>(w,h, data,other.data, compDst, compSrc); return;
          case 4: noncompresedConv<uint16_t,float>   (w,h, data,other.data, compDst, compSrc); return;
          }
        }
      case 4:{
        switch(byteSrc) {
          case 1: noncompresedConv<float,uint8_t> (w,h, data,other.data, compDst, compSrc); return;
          case 2: noncompresedConv<float,uint16_t>(w,h, data,other.data, compDst, compSrc); return;
          case 4: noncompresedConv<float,float>   (w,h, data,other.data, compDst, compSrc); return;
          }
        }
      }

    // unreachable
    throw std::runtime_error("unimplemented");
    }

  Impl(IDevice& f){
    uint32_t bpp = 0;
    frm  = Pixmap::Format::RGBA;
    data = PixmapCodec::loadImg(f,w,h,frm,mipCnt,bpp,dataSz);

    if(data==nullptr && bpp==0)
      throw std::system_error(Tempest::SystemErrc::UnableToLoadAsset);
    }

  ~Impl(){
    PixmapCodec::freeImg(data);
    }

  static size_t calcDataSize(uint32_t w, uint32_t h, Format frm) {
    auto bsz = blockCount(frm,w,h);
    auto bpb = blockSizeForFormat(frm);
    return size_t(bsz.w)*size_t(bsz.h)*size_t(bpb);
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

  template<class T>
  static T maxColor(T*) {
    return T(-1);
    }

  static float maxColor(float*) {
    return float(1.0);
    }

  template<class Tout, class Tin>
  static void noncompresedConv(size_t w, size_t h, void* vdata, const void* vsrc, uint8_t eltOut, uint8_t eltIn){
    auto*             data = reinterpret_cast<Tout*>(vdata);
    auto*             src  = reinterpret_cast<const Tin*>(vsrc);
    static const auto one  = maxColor(reinterpret_cast<Tout*>(0));

    const size_t size = w*h;
    for(size_t i=0;i<size;++i) {
      Tout*       pix = data+i*eltOut;
      const Tin*  s   = src +i*eltIn;

      Tout tmp[4] = {0,0,0,one};
      for(uint8_t i=0;i<eltIn;++i)
        copy(tmp[i],s[i]);

      for(uint8_t i=0;i<eltOut;++i)
        copy(pix[i],tmp[i]);
      }
    }

  static void copy(uint8_t& r,uint8_t v){
    r = v;
    }
  static void copy(uint8_t& r,uint16_t v){
    r = v/256;
    }
  static void copy(uint8_t& r,float v){
    r = uint8_t(std::fmax(0.f,std::fmin(v,1.f))*255.f);
    }
  static void copy(uint16_t& r,uint16_t v){
    r = v;
    }
  static void copy(uint16_t& r,uint8_t v){
    r = v*256+255*(v%2);
    }
  static void copy(uint16_t& r,float v){
    r = uint16_t(std::fmax(0.f,std::fmin(v,1.f))*65535);
    }
  static void copy(float& r,uint8_t v){
    r = v/255.f;
    }
  static void copy(float& r,uint16_t v){
    r = v/65535.f;
    }
  static void copy(float& r,float v){
    r = v;
    }

  static uint8_t bytesPerChannel(Pixmap::Format frm) {
    switch(frm) {
      case Pixmap::Format::DXT1:    return 0;
      case Pixmap::Format::DXT3:    return 0;
      case Pixmap::Format::DXT5:    return 0;
      //---
      default:
        return uint8_t(Pixmap::bppForFormat(frm)/Pixmap::componentCount(frm));
      }
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
  size_t         dataSz = 0;
  Pixmap::Format frm    = Pixmap::Format::RGB;
  uint32_t       mipCnt = 1;

  static Impl    zero;
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
  if(ext==nullptr) {
    for(size_t i=0; path[i]; ++i)
      if(path[i]=='.')
        ext = (path+i+1);
    }
  if(ext!=nullptr && ext[0]=='\0')
    ext = nullptr;

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
  return uint32_t(bppForFormat(impl->frm));
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

size_t Pixmap::bppForFormat(Format f) {
  if(Impl::isCompressed(f))
    return 0;
  return blockSizeForFormat(f);
  }

size_t Pixmap::blockSizeForFormat(Pixmap::Format frm) {
  switch(frm) {
    case Pixmap::Format::R:       return 1;
    case Pixmap::Format::RG:      return 2;
    case Pixmap::Format::RGB:     return 3;
    case Pixmap::Format::RGBA:    return 4;
    //---
    case Pixmap::Format::R16:     return 2;
    case Pixmap::Format::RG16:    return 4;
    case Pixmap::Format::RGB16:   return 6;
    case Pixmap::Format::RGBA16:  return 8;
    //---
    case Pixmap::Format::R32F:    return 4;
    case Pixmap::Format::RG32F:   return 8;
    case Pixmap::Format::RGB32F:  return 12;
    case Pixmap::Format::RGBA32F: return 16;
    //---
    case Pixmap::Format::DXT1:    return 8;
    case Pixmap::Format::DXT3:    return 16;
    case Pixmap::Format::DXT5:    return 16;
    }
  return 0;
  }

uint8_t Pixmap::componentCount(Pixmap::Format frm) {
  switch(frm) {
    case Pixmap::Format::R:       return 1;
    case Pixmap::Format::RG:      return 2;
    case Pixmap::Format::RGB:     return 3;
    case Pixmap::Format::RGBA:    return 4;
    //---
    case Pixmap::Format::R16:     return 1;
    case Pixmap::Format::RG16:    return 2;
    case Pixmap::Format::RGB16:   return 3;
    case Pixmap::Format::RGBA16:  return 4;
    //---
    case Pixmap::Format::R32F:    return 1;
    case Pixmap::Format::RG32F:   return 2;
    case Pixmap::Format::RGB32F:  return 3;
    case Pixmap::Format::RGBA32F: return 4;
    //---
    case Pixmap::Format::DXT1:    return 3;
    case Pixmap::Format::DXT3:    return 4;
    case Pixmap::Format::DXT5:    return 4;
    }
  return 0;
  }

Size Pixmap::blockCount(Format frm, uint32_t w, uint32_t h) {
  switch(frm) {
    case Pixmap::Format::R:
    case Pixmap::Format::RG:
    case Pixmap::Format::RGB:
    case Pixmap::Format::RGBA:
    case Pixmap::Format::R16:
    case Pixmap::Format::RG16:
    case Pixmap::Format::RGB16:
    case Pixmap::Format::RGBA16:
    case Pixmap::Format::R32F:
    case Pixmap::Format::RG32F:
    case Pixmap::Format::RGB32F:
    case Pixmap::Format::RGBA32F:
      return Size(w,h);
    case Pixmap::Format::DXT1:
    case Pixmap::Format::DXT3:
    case Pixmap::Format::DXT5:
      return Size((w+3)/4,(h+3)/4);
    }
  return Size(0,0);
  }

TextureFormat Pixmap::toTextureFormat(Pixmap::Format f) {
  switch(f) {
    case Pixmap::Format::R:       return TextureFormat::R8;
    case Pixmap::Format::RG:      return TextureFormat::RG8;
    case Pixmap::Format::RGB:     return TextureFormat::RGB8;
    case Pixmap::Format::RGBA:    return TextureFormat::RGBA8;

    case Pixmap::Format::R16:     return TextureFormat::R16;
    case Pixmap::Format::RG16:    return TextureFormat::RG16;
    case Pixmap::Format::RGB16:   return TextureFormat::RGB16;
    case Pixmap::Format::RGBA16:  return TextureFormat::RGBA16;

    case Pixmap::Format::R32F:    return TextureFormat::R32F;
    case Pixmap::Format::RG32F:   return TextureFormat::RG32F;
    case Pixmap::Format::RGB32F:  return TextureFormat::RGB32F;
    case Pixmap::Format::RGBA32F: return TextureFormat::RGBA32F;

    case Pixmap::Format::DXT1:    return TextureFormat::DXT1;
    case Pixmap::Format::DXT3:    return TextureFormat::DXT3;
    case Pixmap::Format::DXT5:    return TextureFormat::DXT5;
    }
  return TextureFormat::Undefined;
  }

Pixmap::Format Pixmap::toPixmapFormat(TextureFormat f) {
  switch(f) {
    //---
    case TextureFormat::R8:        return Pixmap::Format::R;
    case TextureFormat::RG8:       return Pixmap::Format::RG;
    case TextureFormat::RGB8:      return Pixmap::Format::RGB;
    case TextureFormat::RGBA8:     return Pixmap::Format::RGBA;
    //---
    case TextureFormat::R16:       return Pixmap::Format::R16;
    case TextureFormat::RG16:      return Pixmap::Format::RG16;
    case TextureFormat::RGB16:     return Pixmap::Format::RGB16;
    case TextureFormat::RGBA16:    return Pixmap::Format::RGBA16;
    //---
    case TextureFormat::R32F:      return Pixmap::Format::R32F;
    case TextureFormat::RG32F:     return Pixmap::Format::RG32F;
    case TextureFormat::RGB32F:    return Pixmap::Format::RGB32F;
    case TextureFormat::RGBA32F:   return Pixmap::Format::RGBA32F;
    //---
    case TextureFormat::DXT1:      return Pixmap::Format::DXT1;
    case TextureFormat::DXT3:      return Pixmap::Format::DXT3;
    case TextureFormat::DXT5:      return Pixmap::Format::DXT5;
    //---
    case TextureFormat::R11G11B10UF:
    case TextureFormat::RGBA16F:
      break; //fallthrough to exception
    //---
    case TextureFormat::Depth16:   return Pixmap::Format::R16;
    case TextureFormat::Depth24S8:
    case TextureFormat::Depth24x8:
    case TextureFormat::Undefined:
    case TextureFormat::Last:
      break; //fallthrough to exception
    }
  throw std::runtime_error("cannot convert depth format to pixmap format");
  }
