#pragma once

#include "../pixmapcodec.h"

namespace Tempest {

class PixmapCodecCommon : public PixmapCodec {
  public:
    PixmapCodecCommon();

  protected:
    bool     testFormat(const Context& c) const override;
    uint8_t* load(PixmapCodec::Context &c,uint32_t& w,uint32_t& h,TextureFormat& frm,uint32_t& mipCnt,size_t& dataSz,uint32_t& bpp) const override;
    bool     save(ODevice& f, const char* ext, const uint8_t *data, size_t dataSz, uint32_t w, uint32_t h, TextureFormat frm) const override;
  };

}

