#pragma once

#include <Tempest/AbstractGraphicsApi>
#include "utility/spinlock.h"
#include "mtswapchainstate.h"
#include "nsptr.h"

#include <Metal/Metal.hpp>

#include <memory>
#include <vector>

namespace CA
{
class MetalDrawable;
}

namespace Tempest {
namespace Detail {

class MtDevice;

struct MtSwapchainFrame final {
  MtSwapchainFrame(uint64_t generation, uint32_t image, bool direct,
                   MTL::Texture* texture, CA::MetalDrawable* drawable);
  ~MtSwapchainFrame();

  MtSwapchainFrame(const MtSwapchainFrame&) = delete;
  MtSwapchainFrame& operator=(const MtSwapchainFrame&) = delete;

  uint64_t                 generation = 0;
  uint32_t                 image      = 0;
  bool                     direct     = false;
  NsPtr<MTL::Texture>      texture;
  NsPtr<CA::MetalDrawable> drawable;
  };

class MtSwapchain : public AbstractGraphicsApi::Swapchain {
  public:
    using Frame = std::shared_ptr<MtSwapchainFrame>;

    MtSwapchain(MtDevice& dev, SystemApi::Window* w, uint32_t bufferCount,
                bool directPreferred);
    ~MtSwapchain();

    void          reset() override;
    uint32_t      currentBackBufferIndex() override;
    uint32_t      imageCount() const override;
    uint32_t      w() const override;
    uint32_t      h() const override;
    void          present();
    NonUniqResId  syncId() const override { return NonUniqResId::I_None; }

    Frame         acquireRenderTarget(uint32_t image);
    MTL::PixelFormat format() const;

  private:
    struct Image {
      NsPtr<MTL::Texture> tex;
      };
    struct Impl;
    std::unique_ptr<Impl> pimpl;

    mutable SpinLock      sync;
    MtDevice&             dev;
    Tempest::Size         sz;

    std::vector<Image>    img;
    MtSwapchainState      state;
    MtSwapchainOperationGate operationGate;
    Frame                 activeFrame;
    bool                  directPreferred = false;

    NsPtr<MTL::Texture>   mkTexture(const Tempest::Size& size);
    Frame                 acquireRenderTargetImpl(uint32_t image);
    Frame                 acquireCopyFrame(const MtSwapchainState::Ticket& ticket);
  };

}
}
