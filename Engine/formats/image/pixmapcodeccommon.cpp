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

using namespace Tempest;

static int stbiEof(void* /*user*/) {
  //return reinterpret_cast<IDevice*>(user)->peek();
  return 0;
  }

static int stbiRead(void* user, char* data, int size) {
  return int(reinterpret_cast<IDevice*>(user)->read(data,size_t(size)));
  }

static void stbiSkip(void* user, int n) {
  reinterpret_cast<IDevice*>(user)->seek(size_t(n));
  }

static void stbi__start_file(stbi__context *s, IDevice *f) {
  static stbi_io_callbacks stbi__stdio_callbacks = {
    stbiRead,
    stbiSkip,
    stbiEof,
    };

  stbi__start_callbacks(s,&stbi__stdio_callbacks,reinterpret_cast<void*>(f));
  }

PixmapCodecCommon::PixmapCodecCommon() {
  }

bool PixmapCodecCommon::testFormat(const Tempest::PixmapCodec::Context &ctx) const {
  char buf[128]={};
  size_t bufSiz = std::min(sizeof(buf),ctx.bufferSize());
  ctx.peek(buf,bufSiz);
  Tempest::MemReader dev(buf,bufSiz);

  stbi__context s;
  stbi__start_file(&s,&dev);
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
                                 Pixmap::Format &frm, uint32_t &mipCnt, size_t &dataSz, uint32_t &obpp) const {
  auto& f = ctx.device;
  stbi__context s;
  stbi__start_file(&s,&f);

  int w=0,h=0,bpp=0;
  uint8_t *result = stbi__load_and_postprocess_8bit(&s,&w,&h,&bpp,STBI_default);
  if( result ) {
    // need to 'unget' all the characters in the IO buffer
    size_t extra = size_t(s.img_buffer_end-s.img_buffer);
    if(f.unget(extra)!=extra)
      throw std::system_error(Tempest::SystemErrc::UnableToLoadAsset);
    }

  if(bpp==3) {
    frm    = Pixmap::Format::RGB;
    dataSz = size_t(w*h*3);
    } else {
    frm    = Pixmap::Format::RGBA;
    dataSz = size_t(w*h*4);
    }

  ow     = uint32_t(w);
  oh     = uint32_t(h);
  obpp   = uint32_t(bpp);
  mipCnt = 1;
  return result;
  }

bool PixmapCodecCommon::save(ODevice &f, const char *ext, const uint8_t* cdata,
                             size_t dataSz, uint32_t w, uint32_t h, Pixmap::Format frm) const {
  (void)dataSz;

  int bpp=0;
  switch(frm) {
    case Pixmap::Format::A:    bpp=1; break;
    case Pixmap::Format::RGB:  bpp=3; break;
    case Pixmap::Format::RGBA: bpp=4; break;
    case Pixmap::Format::DXT1: bpp=0; break;
    case Pixmap::Format::DXT3: bpp=0; break;
    case Pixmap::Format::DXT5: bpp=0; break;
    }

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
    ODevice* dev    = nullptr;
    bool     badbit = false;

    static void write(void* ctx,void* data,int len) {
      WContext* c = reinterpret_cast<WContext*>(ctx);
      c->badbit = c->badbit || (c->dev->write(data,size_t(len))!=size_t(len));
      }
    };

  WContext ctx = {&f,false};
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
