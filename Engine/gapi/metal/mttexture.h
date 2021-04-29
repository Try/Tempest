#pragma once

#include <Tempest/AbstractGraphicsApi>
#include "nsptr.h"

namespace Tempest {
namespace Detail {

class MtDevice;

class MtTexture : public Tempest::AbstractGraphicsApi::Texture {
  public:
    MtTexture(MtDevice &d,
              const uint32_t w, const uint32_t h, uint32_t mips, TextureFormat frm);

    uint32_t mipCount() const override;

    NsPtr impl;
    const  uint32_t mips = 0;
  };

}
}