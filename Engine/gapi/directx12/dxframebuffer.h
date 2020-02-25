#pragma once

#include <Tempest/AbstractGraphicsApi>

#include <d3d12.h>
#include "gapi/directx12/comptr.h"

namespace Tempest {

namespace Detail {

class DxDevice;
class DxSwapchain;

class DxFramebuffer : public AbstractGraphicsApi::Fbo {
  public:
    DxFramebuffer(DxDevice& dev, DxSwapchain &swapchain, size_t image);
    ~DxFramebuffer();

    ComPtr<ID3D12DescriptorHeap>              rtvHeap;

    std::unique_ptr<ComPtr<ID3D12Resource>[]> views;
    UINT32                                    viewsCount;
    ComPtr<ID3D12Resource>                    ds;
  };
}
}


