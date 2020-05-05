#pragma once

#include <Tempest/AbstractGraphicsApi>

#include <d3d12.h>
#include "gapi/directx12/comptr.h"

namespace Tempest {

namespace Detail {

class DxDevice;
class DxTexture;
class DxSwapchain;

class DxFramebuffer : public AbstractGraphicsApi::Fbo {
  public:
    DxFramebuffer(DxDevice& dev, DxSwapchain &swapchain, size_t image);
    DxFramebuffer(DxDevice& dev, DxSwapchain &swapchain, size_t image, DxTexture &zbuf);
    DxFramebuffer(DxDevice& dev, DxTexture &cl, DxTexture &zbuf);
    DxFramebuffer(DxDevice& dev, DxTexture &cl);
    ~DxFramebuffer();

    ComPtr<ID3D12DescriptorHeap>              rtvHeap;

    std::unique_ptr<ID3D12Resource*[]>        views;
    UINT32                                    viewsCount;

  private:
    ComPtr<ID3D12Resource>                    swapchainImg;
  };
}
}


