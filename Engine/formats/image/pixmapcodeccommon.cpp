#include "pixmapcodeccommon.h"

#include <Tempest/IDevice>
#include <Tempest/ODevice>
#include <Tempest/MemReader>
#include <Tempest/Except>

#include <cstring>
#include <algorithm>

#define STBI_MALLOC(sz)           malloc(sz)
#define STBI_REALLOC(p,newsz)     realloc(p,newsz)
#define STBI_FREE(p)              free(p)

#include "thirdparty/stb_image.h"
#include "thirdparty/stb_image_write.h"

namespace Tempest {
namespace Detail {
struct StbContext final {
  IDevice& device;
  bool     err = false;
  };
}
};

using namespace Tempest;
using namespace Tempest::Detail;

static int stbiEof(void* /*user*/) {
  //return reinterpret_cast<IDevice*>(user)->peek();
  return 0;
  }

static int stbiRead(void* user, char* data, int size) {
  auto& ctx = *reinterpret_cast<StbContext*>(user);
  return ctx.err ? 0 : int(ctx.device.read(data,size_t(size)));
  }

static void stbiSkip(void* user, int n) {
  auto& ctx = *reinterpret_cast<StbContext*>(user);
  if(ctx.err)
    return;
  ctx.err |= (size_t(n)!=ctx.device.seek(size_t(n)));
  }

static void stbi__start_file(stbi__context *s, StbContext *f) {
  static stbi_io_callbacks stbi__stdio_callbacks = {
    stbiRead,
    stbiSkip,
    stbiEof,
    };

  stbi__start_callbacks(s,&stbi__stdio_callbacks,reinterpret_cast<void*>(f));
  }

static uint8_t* loadUnorm(stbi__context& s, uint32_t &ow, uint32_t &oh, Pixmap::Format &frm) {
  int w=0,h=0,compCnt=0;
  stbi__result_info ri;

  uint8_t *result = reinterpret_cast<uint8_t*>(stbi__load_main(&s, &w, &h, &compCnt, STBI_default, &ri, 8));
  if(result==nullptr)
    return nullptr;

  if(ri.bits_per_channel==8) {
    frm = Pixmap::Format(int(Pixmap::Format::R)+compCnt-1);
    }
  else if(ri.bits_per_channel==16) {
    frm = Pixmap::Format(int(Pixmap::Format::R16)+compCnt-1);
    }
  else {
    std::free(result);
    return nullptr;
    }

  ow = uint32_t(w);
  oh = uint32_t(h);
  return result;
  }

static uint8_t* loadFloat(stbi__context& s, uint32_t &ow, uint32_t &oh, Pixmap::Format &frm) {
  int w=0, h=0, compCnt=0;
  float* result = stbi__loadf_main(&s, &w, &h, &compCnt, STBI_default);
  if(result==nullptr)
    return nullptr;
  ow  = uint32_t(w);
  oh  = uint32_t(h);
  switch(compCnt) {
    case 1:
      frm = Pixmap::Format::R32F;
      break;
    case 2:
      frm = Pixmap::Format::RG32F;
      break;
    case 3:
      frm = Pixmap::Format::RGB32F;
      break;
    case 4:
      frm = Pixmap::Format::RGBA32F;
      break;
    default:
      std::free(result);
      return nullptr;
    }
  return reinterpret_cast<uint8_t*>(result);
  }

PixmapCodecCommon::PixmapCodecCommon() {
  }

bool PixmapCodecCommon::testFormat(const Tempest::PixmapCodec::Context &ctx) const {
  char buf[128]={};
  size_t bufSiz = std::min(sizeof(buf),ctx.bufferSize());
  ctx.peek(buf,bufSiz);
  Tempest::MemReader dev(buf,bufSiz);
  StbContext f = {dev,false};

  stbi__context s;
  stbi__start_file(&s,&f);
  if(stbi__png_test(&s))
    return true;
  if(stbi__jpeg_test(&s))
    return true;
  if(stbi__bmp_test(&s))
    return true;
  if(stbi__gif_test(&s))
    return true;
  if(stbi__psd_test(&s))
    return true;
  if(stbi__pic_test(&s))
    return true;
  if(stbi__pnm_test(&s))
    return true;
  if(stbi__hdr_test(&s))
    return true;
  if(stbi__tga_test(&s))
    return true;

  return false;
  }

uint8_t *PixmapCodecCommon::load(PixmapCodec::Context &ctx, uint32_t &ow, uint32_t &oh,
                                 Pixmap::Format &frm, uint32_t &mipCnt, size_t &dataSz, uint32_t &bpp) const {
  StbContext f = {ctx.device,false};
  stbi__context s;
  stbi__start_file(&s,&f);

  uint8_t* result = nullptr;
  if(stbi__hdr_test(&s)) {
    result = loadFloat(s,ow,oh,frm);
    } else {
    result = loadUnorm(s,ow,oh,frm);
    }
  if(result==nullptr)
    return nullptr;
  // need to 'unget' all the characters in the IO buffer
  size_t extra = size_t(s.img_buffer_end-s.img_buffer);
  if(f.err || f.device.unget(extra)!=extra)
    throw std::system_error(Tempest::SystemErrc::UnableToLoadAsset);

  bpp = 0;
  switch(frm) {
    case Pixmap::Format::R:
      bpp = 1;
      break;
    case Pixmap::Format::RG:
      bpp = 2;
      break;
    case Pixmap::Format::RGB:
      bpp = 3;
      break;
    case Pixmap::Format::RGBA:
      bpp = 4;
      break;
    case Pixmap::Format::R16:
      bpp = 2;
      break;
    case Pixmap::Format::RG16:
      bpp = 4;
      break;
    case Pixmap::Format::RGB16:
      bpp = 6;
      break;
    case Pixmap::Format::RGBA16:
      bpp = 8;
      break;
    case Pixmap::Format::R32F:
      bpp = sizeof(float);
      break;
    case Pixmap::Format::RG32F:
      bpp = 2*sizeof(float);
      break;
    case Pixmap::Format::RGB32F:
      bpp = 3*sizeof(float);
      break;
    case Pixmap::Format::RGBA32F:
      bpp = 4*sizeof(float);
      break;
    case Pixmap::Format::DXT1:
    case Pixmap::Format::DXT3:
    case Pixmap::Format::DXT5:
      // not supported by common codec
      throw std::system_error(Tempest::SystemErrc::UnableToLoadAsset);
    }
  dataSz = size_t(ow*oh*bpp);
  mipCnt = 1;
  return result;
  }

bool PixmapCodecCommon::save(ODevice &f, const char *ext, const uint8_t* cdata,
                             size_t dataSz, uint32_t w, uint32_t h, Pixmap::Format frm) const {
  (void)dataSz;

  int bpp = int(Pixmap::bppForFormat(frm));
  if(bpp==0)
    return false;

  uint8_t *data = const_cast<uint8_t*>(cdata);

  if(ext==nullptr || std::strcmp("png",ext)==0) {
    int len=0;
    int stride_bytes=0;
    uint8_t* dat = stbi_write_png_to_mem(data,stride_bytes,int(w),int(h),int(bpp),&len);
    if(dat==nullptr)
      return false;

    bool ret=(f.write(dat,size_t(len))==size_t(len) && f.flush());
    std::free(dat);
    return ret;
    }

  struct WContext final {
    WContext(ODevice* dev, bool badbit):dev(dev),badbit(badbit){}
    ODevice* dev    = nullptr;
    bool     badbit = false;

    static void write(void* ctx,void* data,int len) {
      WContext* c = reinterpret_cast<WContext*>(ctx);
      c->badbit = c->badbit || (c->dev->write(data,size_t(len))!=size_t(len));
      }
    };

  WContext ctx = WContext{&f,false};
  if(std::strcmp("jpg",ext)==0 || std::strcmp("jpeg",ext)==0) {
    stbi_write_jpg_to_func(WContext::write,&ctx,int(w),int(h),int(bpp),cdata,0);
    }
  else if(std::strcmp("bmp",ext)==0) {
    stbi_write_bmp_to_func(WContext::write,&ctx,int(w),int(h),int(bpp),cdata);
    }
  else if(std::strcmp("tga",ext)==0) {
    stbi_write_tga_to_func(WContext::write,&ctx,int(w),int(h),int(bpp),cdata);
    }
  else {
    return false;
    }
  return !ctx.badbit;
  }
