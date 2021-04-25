#pragma once

#include <Tempest/AbstractGraphicsApi>

#include "mtrenderpass.h"
#include "utility/spinlock.h"

@class MTLRenderPassDescriptor;

namespace Tempest {
namespace Detail {

class MtTexture;

class MtFramebuffer : public AbstractGraphicsApi::Fbo {
  public:
    MtFramebuffer(MtTexture **clr, size_t clrSize, MtTexture* depth);
    ~MtFramebuffer();

    struct Inst {
      DSharedPtr<MtRenderPass*> pass;
      MTLRenderPassDescriptor*  desc = nullptr;
      };

    MTLRenderPassDescriptor* instance(MtRenderPass &rp);

    std::vector<MtTexture*> color;
    MtTexture*              depth = nullptr;

  private:
    SpinLock          sync;
    std::vector<Inst> inst;
  };

}
}
