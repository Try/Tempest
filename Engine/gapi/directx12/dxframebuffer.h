#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <cstdlib>
#include <d3d12.h>

#include "gapi/directx12/comptr.h"
#include "dxfbolayout.h"

namespace Tempest {

namespace Detail {

class DxDevice;
class DxTexture;
class DxSwapchain;

class DxFramebuffer : public AbstractGraphicsApi::Fbo {
  public:
    DxFramebuffer(DxDevice& dev, DxFboLayout& lay, uint32_t cnt,
                  DxSwapchain** swapchain, DxTexture** cl, const uint32_t* imgId, DxTexture* zbuf);
    ~DxFramebuffer();

    ComPtr<ID3D12DescriptorHeap>              rtvHeap;
    UINT                                      rtvHeapInc = 0;
    ComPtr<ID3D12DescriptorHeap>              dsvHeap;

    struct Attach final : AbstractGraphicsApi::Attach {
      TextureLayout   defaultLayout() override;
      TextureLayout   renderLayout()  override;
      void*           nativeHandle()  override;

      ID3D12Resource* res       = nullptr;
      bool            isSwImage = false;
      DXGI_FORMAT     format    = DXGI_FORMAT_UNKNOWN;
      };
    std::unique_ptr<Attach[]>  views;
    UINT32                     viewsCount;

    Attach                     depth;
    DSharedPtr<DxFboLayout*>   lay;

  private:
    void setupViews(ID3D12Device& device, ID3D12Resource** res, size_t cnt, ID3D12Resource* ds);
  };
}
}


