#pragma once

#include <Tempest/AbstractGraphicsApi>
#include "nsptr.h"

#include <Metal/Metal.hpp>
#include <mutex>

namespace CA
{
class MetalDrawable;
}

namespace Tempest {
namespace Detail {

class MtDevice;

struct MtSwapchainFrame {
  MtSwapchainFrame(MTL::Texture* texture, CA::MetalDrawable* drawable);
  ~MtSwapchainFrame();

  NsPtr<MTL::Texture>      texture;
  NsPtr<CA::MetalDrawable> drawable;
  };

class MtSwapchain : public AbstractGraphicsApi::Swapchain {
  public:
    using Frame = std::shared_ptr<MtSwapchainFrame>;

    MtSwapchain(MtDevice& dev, SystemApi::Window* w, const Options& options);
    ~MtSwapchain();

    void          reset() override;
    uint32_t      currentBackBufferIndex() override;
    uint32_t      imageCount() const override;
    uint32_t      w() const override;
    uint32_t      h() const override;
    void          present();
    NonUniqResId  syncId() const override { return NonUniqResId::I_None; }

    MTL::PixelFormat format() const;
    Frame         acquireFrame(uint32_t image);

    struct Image {
      NsPtr<MTL::Texture> tex;
      };
    std::vector<Image>    img;

  private:
    struct Impl;
    std::unique_ptr<Impl> pimpl;

    std::mutex            sync;
    MtDevice&             dev;
    Tempest::Size         sz;

    uint32_t              imgCount   = 0;
    uint32_t              currentImg = 0;
    bool                  direct = false;
    Frame                 activeFrame;

    NsPtr<MTL::Texture>   mkTexture();
    void                  nextDrawable();
  };

}
}
