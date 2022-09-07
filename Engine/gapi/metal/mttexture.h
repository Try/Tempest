#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <Metal/Metal.hpp>
#include "nsptr.h"
#include "utility/spinlock.h"

namespace Tempest {
namespace Detail {

class MtDevice;

class MtTexture : public Tempest::AbstractGraphicsApi::Texture {
  public:
    MtTexture(MtDevice &d,
              const uint32_t w, const uint32_t h, const uint32_t depth, uint32_t mips, TextureFormat frm, bool storageTex);
    MtTexture(MtDevice &d, const Pixmap& pm, uint32_t mips, TextureFormat frm);
    ~MtTexture();

    uint32_t mipCount() const override;
    void     readPixels(Pixmap& out, TextureFormat frm,
                        const uint32_t w, const uint32_t h, uint32_t mip);

    uint32_t bitCount();

    MTL::Texture&       view(ComponentMapping m, uint32_t mipLevel);

    MtDevice&           dev;
    NsPtr<MTL::Texture> impl;
    const uint32_t      mipCnt = 0;

  private:
    void createCompressedTexture(MTL::Texture& val, const Pixmap& p, TextureFormat frm, uint32_t mipCnt);
    void createRegularTexture(MTL::Texture& val, const Pixmap& p);

    NsPtr<MTL::Texture>  alloc(TextureFormat frm, const uint32_t w, const uint32_t h, const uint32_t d,
                               const uint32_t mips,
                               MTL::StorageMode smode, MTL::TextureUsage umode);

    struct View {
      ComponentMapping    m;
      uint32_t            mip = uint32_t(0);
      NsPtr<MTL::Texture> v;
      };
    Detail::SpinLock  syncViews;
    std::vector<View> extViews;
  };

}
}
