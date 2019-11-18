#pragma once

#include <Tempest/Pixmap>

#include <vector>
#include <memory>

namespace Tempest {

class IDevice;

class PixmapCodec {
  public:
    PixmapCodec()=default;
    virtual ~PixmapCodec()=default;

    class Context final {
      public:
        explicit Context(IDevice& dev);
        size_t peek(void *out, size_t n) const;
        size_t bufferSize() const { return bufSiz; }

        IDevice& device;

      private:
        size_t  bufSiz=0;
        uint8_t buf[128];
      };

    static uint8_t*  loadImg (IDevice& f, uint32_t& w, uint32_t& h, Pixmap::Format& frm, uint32_t& mipCnt, uint32_t &bpp, size_t& dataSz);
    static void      saveImg (ODevice& f, const char* ext, const uint8_t *data, size_t dataSz, uint32_t w, uint32_t h, Pixmap::Format frm);

    static void      freeImg (uint8_t* px);

  protected:
    virtual bool     testFormat(const Context& c) const = 0;
    virtual uint8_t* load(PixmapCodec::Context &c,uint32_t& w,uint32_t& h,Pixmap::Format& frm,uint32_t& mipCnt,size_t& dataSz,uint32_t& bpp) const = 0;
    virtual bool     save(ODevice& f,const char* ext, const uint8_t *data, size_t dataSz, uint32_t w, uint32_t h, Pixmap::Format frm) const = 0;

  private:
    struct Impl;
    static Impl& instance();
  };

}

