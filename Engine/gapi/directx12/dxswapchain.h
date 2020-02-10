#pragma once

#include <Tempest/AbstractGraphicsApi>

#include <dxgi1_6.h>
#include <d3d12.h>
#include "gapi/directx12/comptr.h"

namespace Tempest {

class Semaphore;

namespace Detail {

class DxDevice;

class DxSwapchain : public AbstractGraphicsApi::Swapchain {
  public:
    DxSwapchain()=default;
    DxSwapchain(DxDevice& device,uint32_t w,uint32_t h);
    DxSwapchain(DxSwapchain&& other);
    ~DxSwapchain() override;

    uint32_t                 w()      const override { return imgW; }
    uint32_t                 h()      const override { return imgH; }

    uint32_t                 imageCount() const override { return imgCount; }
    uint32_t                 nextImage(AbstractGraphicsApi::Semaphore* onReady) override;

  private:
    uint32_t                 currImg=0;
    ComPtr<IDXGISwapChain>   impl;

    uint32_t                 imgW=0, imgH=0;
    uint32_t                 imgCount=3;
  };

}}
