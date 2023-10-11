#pragma once

#include <Tempest/AbstractGraphicsApi>
#include "utility/spinlock.h"
#include "nsptr.h"

#include <Metal/Metal.hpp>

namespace CA
{
class MetalDrawable;
}

namespace Tempest {
namespace Detail {

class MtDevice;

class MtSwapchain : public AbstractGraphicsApi::Swapchain {
  public:
    MtSwapchain(MtDevice& dev, SystemApi::Window* w);
    ~MtSwapchain();

    void          reset() override;
    uint32_t      currentBackBufferIndex() override;
    uint32_t      imageCount() const override;
    uint32_t      w() const override;
    uint32_t      h() const override;
    void          present();

    MTL::PixelFormat format() const;

    struct Image {
      NsPtr<MTL::Texture> tex;
      };
    std::vector<Image>    img;

  private:
    struct Impl;
    std::unique_ptr<Impl> pimpl;

    SpinLock              sync;
    MtDevice&             dev;
    Tempest::Size         sz;

    uint32_t              imgCount   = 0;
    uint32_t              currentImg = 0;

    NsPtr<MTL::Texture>   mkTexture();
    void                  nextDrawable();
  };

}
}
