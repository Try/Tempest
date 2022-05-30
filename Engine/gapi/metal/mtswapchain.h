#pragma once

#include <Tempest/AbstractGraphicsApi>
#include "utility/spinlock.h"
#include "nsptr.h"

#import  <AppKit/AppKit.h>
#include <QuartzCore/QuartzCore.hpp>
#include <Metal/Metal.hpp>

@class MetalView;

namespace Tempest {
namespace Detail {

class MtDevice;

class MtSwapchain : public AbstractGraphicsApi::Swapchain {
  public:
    MtSwapchain(MtDevice& dev, NSWindow *w);
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
      bool                inUse = false;
      };
    std::vector<Image>    img;

  private:
    SpinLock              sync;

    MtDevice&             dev;
    NSWindow*             wnd  = nil;
    MetalView*            view = nil;
    Tempest::Size         sz;

    uint32_t              imgCount   = 0;
    uint32                currentImg = 0;
    NsPtr<MTL::Texture>   mkTexture();
  };

}
}
