#pragma once

#include "../pixmapcodec.h"

namespace Tempest {

class PixmapCodecDDS : public PixmapCodec {
  public:
    PixmapCodecDDS();

  protected:
    bool     testFormat(const Context& c) const override;
    uint8_t* load(PixmapCodec::Context &c,uint32_t& w,uint32_t& h,Pixmap::Format& frm,uint32_t& mipCnt,size_t& dataSz,uint32_t& bpp) const override;
    bool     save(ODevice& f,const char* ext, const uint8_t *data, size_t dataSz, uint32_t w, uint32_t h, Pixmap::Format frm) const override;
  };

}

