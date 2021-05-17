#pragma once

#include <Tempest/AbstractGraphicsApi>
#import  <AppKit/AppKit.h>
#import  <QuartzCore/QuartzCore.h>
#import  <Metal/MTLTexture.h>
#include "utility/spinlock.h"

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

    MTLPixelFormat format() const;

    struct Image {
      id<MTLTexture>      tex      = nil;
      bool                inUse    = false;
      };
    std::vector<Image> img;

  private:
    SpinLock       sync;

    MtDevice&      dev;
    NSWindow*      wnd  = nil;
    MetalView*     view = nil;
    Tempest::Size  sz;

    uint32_t       imgCount   = 0;
    uint32         currentImg = 0;
    void           releaseTex();
    id<MTLTexture> mkTexture();
  };

}
}
