#pragma once

#include <Tempest/AbstractGraphicsApi>
#import  <AppKit/AppKit.h>
#import  <QuartzCore/QuartzCore.h>
#import  <Metal/MTLTexture.h>

@class MetalView;

namespace Tempest {
namespace Detail {

class MtDevice;

class MtSwapchain : public AbstractGraphicsApi::Swapchain {
  public:
    MtSwapchain(MtDevice& dev, NSWindow *w);
    ~MtSwapchain();

    void          reset() override;
    uint32_t      nextImage(AbstractGraphicsApi::Semaphore* onReady) override;
    uint32_t      imageCount() const override;
    uint32_t      w() const override;
    uint32_t      h() const override;

    MTLPixelFormat format() const;
    void           releaseImg();

    id<CAMetalDrawable>               current = nil;
    std::unique_ptr<id<MTLTexture>[]> img     = {};

  private:
    NSWindow*      wnd  = nil;
    MetalView*     view = nil;
    Tempest::Size  sz;

    uint32_t       imgCount = 0;
  };

}
}
