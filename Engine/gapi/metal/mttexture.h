#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <Metal/MTLTexture.h>

namespace Tempest {
namespace Detail {

class MtDevice;

class MtTexture : public Tempest::AbstractGraphicsApi::Texture {
  public:
    MtTexture(MtDevice &d,
              const uint32_t w, const uint32_t h, uint32_t mips, TextureFormat frm);
    MtTexture(MtDevice &d, const Pixmap& pm, uint32_t mips, TextureFormat frm);
    ~MtTexture();

    uint32_t mipCount() const override;

    id<MTLTexture> impl;
    const uint32_t mips = 0;
  };

}
}
