#pragma once

#include <Tempest/AbstractGraphicsApi>
#include "gapi/metal/mttexture.h"
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
    auto          currentBackBuffer() -> AbstractGraphicsApi::Texture* override;
    uint32_t      w() const override;
    uint32_t      h() const override;
    void          present();

    MTL::PixelFormat format() const;

  private:
    struct Impl;
    std::unique_ptr<Impl> pimpl;

    MtDevice&             dev;
    Tempest::Size         sz;
    MtTextureSwapcain     currentAtt;
    NsPtr<CA::MetalDrawable> currentDrawable;

    uint32_t              imgCount   = 0;
  };

}
}
