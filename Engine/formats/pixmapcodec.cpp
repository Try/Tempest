#include "pixmapcodec.h"

#include "image/pixmapcodeccommon.h"
#include "image/pixmapcodecdds.h"

#include <Tempest/IDevice>
#include <Tempest/Except>

#include <cstring>
#include <squish.h>

#include "ddsdef.h"

using namespace Tempest;

PixmapCodec::Context::Context(IDevice &dev)
  :device(dev) {
  bufSiz = dev.read(buf,sizeof(buf));
  if(dev.unget(bufSiz)!=bufSiz)
    throw std::system_error(Tempest::SystemErrc::UnableToLoadAsset);
  }

size_t PixmapCodec::Context::peek(void* out,size_t n) const {
  if(n>bufSiz)
    n = bufSiz;
  std::memcpy(out,buf,n);
  return n;
  }

struct PixmapCodec::Impl {
  Impl() {
    // thread-safe init, because PixmapCodec::instance
    codec.emplace_back(std::make_unique<PixmapCodecDDS>());
    codec.emplace_back(std::make_unique<PixmapCodecCommon>());
    }

  uint8_t*  load(IDevice& f, uint32_t& w, uint32_t& h, Pixmap::Format& frm, uint32_t& mipCnt, uint32_t& bpp, size_t& dataSz) {
    Context ctx(f);

    for(auto& i:codec)
      if(i->testFormat(ctx)) {
        uint8_t* ret = i->load(ctx,w,h,frm,mipCnt,dataSz,bpp);
        if(ret!=nullptr)
          return ret;
        }

    throw std::system_error(Tempest::SystemErrc::UnableToLoadAsset);
    }

  void implSave(ODevice &f, char *ext, const uint8_t *data, size_t dataSz, uint32_t w, uint32_t h, Pixmap::Format frm) {
    if(ext!=nullptr) {
      for(size_t i=0;ext[i];++i)
        if('A'<=ext[i] && ext[i]<='Z')
          ext[i] = ext[i]+'a'-'A';

      for(auto& i:codec) {
        if(i->save(f,ext,data,dataSz,w,h,frm))
          return;
        }
      }

    for(auto& i:codec) {
      if(i->save(f,nullptr,data,dataSz,w,h,frm))
        return;
      }

    throw std::system_error(Tempest::SystemErrc::UnableToSaveAsset);
    }

  void save(ODevice &f, const char *ext, const uint8_t *data, size_t dataSz, uint32_t w, uint32_t h, Pixmap::Format frm) {
    if(ext==nullptr) {
      implSave(f,nullptr,data,dataSz,w,h,frm);
      return;
      }

    size_t extL = std::strlen(ext);
    if(extL<32) {
      char e[33]={};
      std::memcpy(e,ext,extL);
      implSave(f,e,data,dataSz,w,h,frm);
      } else {
      std::unique_ptr<char[]> e(new char[extL+1]);
      std::memcpy(e.get(),ext,extL);
      implSave(f,e.get(),data,dataSz,w,h,frm);
      }
    }

  std::vector<std::unique_ptr<PixmapCodec>> codec;
  };

PixmapCodec::Impl &PixmapCodec::instance() {
  static Impl inst;
  return inst;
  }

uint8_t* PixmapCodec::loadImg(IDevice &f, uint32_t &w, uint32_t &h, Pixmap::Format &frm, uint32_t &mipCnt, uint32_t &bpp, size_t &dataSz) {
  return instance().load(f,w,h,frm,mipCnt,bpp,dataSz);
  }

void PixmapCodec::saveImg(ODevice &f, const char *ext, const uint8_t *data, size_t dataSz, uint32_t w, uint32_t h, Pixmap::Format frm) {
  instance().save(f,ext,data,dataSz,w,h,frm);
  }

void PixmapCodec::freeImg(uint8_t *px) {
  std::free(px);
  }
