#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <d3d12.h>

#include "gapi/directx12/comptr.h"
#include "gapi/directx12/dxdescriptorallocator.h"
#include "utility/spinlock.h"

namespace Tempest {

namespace Detail {

class DxDevice;

class DxTexture : public AbstractGraphicsApi::Texture {
  public:
    DxTexture(DxDevice& dev, ComPtr<ID3D12Resource>&& b, DXGI_FORMAT frm, NonUniqResId nonUniqId,
              UINT mipCnt, UINT sliceCnt, bool is3D);
    DxTexture(DxTexture&& other);
    ~DxTexture();
    DxTexture& operator=(const DxTexture& other) = delete;

    auto     view(DxDevice& dev, const ComponentMapping& m, uint32_t mipLevel, bool is3D) -> D3D12_CPU_DESCRIPTOR_HANDLE;
    uint32_t mipCount() const override { return mipCnt; }

    UINT     bitCount() const;
    UINT     bytePerBlockCount() const;

    DxDevice*              device       = nullptr;
    ComPtr<ID3D12Resource> impl;
    DXGI_FORMAT            format       = DXGI_FORMAT_UNKNOWN;
    NonUniqResId           nonUniqId    = NonUniqResId::I_None;
    UINT                   mipCnt       = 1;
    UINT                   sliceCnt     = 1;
    bool                   is3D         = false;
    bool                   isFilterable = false;

  private:
    void createView(D3D12_CPU_DESCRIPTOR_HANDLE ret, DxDevice& dev, DXGI_FORMAT format,
                     const ComponentMapping* cmap, uint32_t mipLevel, bool is3D);

    struct View {
      ComponentMapping                  m;
      uint32_t                          mip  = uint32_t(0);
      bool                              is3D = false;
      DxDescriptorAllocator::Allocation v;
      };
    Detail::SpinLock  syncViews;
    std::vector<View> extViews;

    DxDescriptorAllocator::Allocation imgView;
  };

class DxTextureWithRT : public DxTexture {
  public:
    DxTextureWithRT(DxDevice& dev, DxTexture&& base);

    ComPtr<ID3D12DescriptorHeap> heap;
    D3D12_CPU_DESCRIPTOR_HANDLE  handle;
    D3D12_CPU_DESCRIPTOR_HANDLE  handleR;
  };

}
}

