#pragma once

#include <Tempest/AbstractGraphicsApi>

#include "mtfbolayout.h"
#include "mtrenderpass.h"

#include "utility/spinlock.h"

#import  <Metal/MTLTexture.h>

@class MTLRenderPassDescriptor;

namespace Tempest {
namespace Detail {

class MtTexture;

class MtFramebuffer : public AbstractGraphicsApi::Fbo {
  public:
    MtFramebuffer(MtFboLayout& lay, MtSwapchain **sw, const uint32_t *imgId,
                  MtTexture **clr, size_t clrSize,
                  MtTexture* depth);
    ~MtFramebuffer();

    struct Inst {
      DSharedPtr<MtRenderPass*> pass;
      MTLRenderPassDescriptor*  desc = nullptr;
      };

    struct Attach {
      id<MTLTexture> id;
      };

    MTLRenderPassDescriptor* instance(MtRenderPass &rp);

    DSharedPtr<MtFboLayout*> layout;
    std::vector<Attach>      color;
    MtTexture*               depth = nullptr;

  private:
    SpinLock          sync;
    std::vector<Inst> inst;
  };

}
}
