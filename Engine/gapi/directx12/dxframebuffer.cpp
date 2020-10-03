#if defined(TEMPEST_BUILD_DIRECTX12)

#include "dxframebuffer.h"
#include "dxdevice.h"
#include "dxswapchain.h"
#include "dxtexture.h"
#include "guid.h"

using namespace Tempest;
using namespace Tempest::Detail;

Tempest::Detail::DxFramebuffer::DxFramebuffer(DxDevice& dev, DxFboLayout& lay, uint32_t cnt,
                                              DxSwapchain** swapchain, DxTexture** cl, const uint32_t* imgId, DxTexture* zbuf)
  :viewsCount(cnt), lay(&lay) {
  ID3D12Resource* res[256] = {};
  for(size_t i=0; i<cnt; ++i) {
    if(cl[i]==nullptr) {
      res[i] = swapchain[i]->views[imgId[i]].get();
      } else {
      res[i] = cl[i]->impl.get();
      }
    }
  setupViews(*dev.device, res, cnt, zbuf==nullptr ? nullptr : zbuf->impl.get());
  for(size_t i=0; i<cnt; ++i)
    if(cl[i]==nullptr)
      views[i].isSwImage = true;
  }

DxFramebuffer::~DxFramebuffer() {
  }

void DxFramebuffer::setupViews(ID3D12Device& device,
                               ID3D12Resource** res, size_t cnt, ID3D12Resource* ds) {
  // descriptor heap
  D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
  rtvHeapDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
  rtvHeapDesc.NumDescriptors = UINT(cnt);
  rtvHeapDesc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
  dxAssert(device.CreateDescriptorHeap(&rtvHeapDesc, uuid<ID3D12DescriptorHeap>(), reinterpret_cast<void**>(&rtvHeap)));

  // frame resources.
  views.reset(new View[viewsCount]);
  for(size_t i=0;i<viewsCount;++i) {
    views[i].res = res[i];
    auto desc = views[i].res->GetDesc();
    views[i].format = desc.Format;
    }

  if(ds!=nullptr) {
    D3D12_DESCRIPTOR_HEAP_DESC dsHeapDesc = {};
    dsHeapDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    dsHeapDesc.NumDescriptors = 1;//UINT(cnt + (ds==nullptr ? 0 : 1));
    dsHeapDesc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    dxAssert(device.CreateDescriptorHeap(&dsHeapDesc, uuid<ID3D12DescriptorHeap>(), reinterpret_cast<void**>(&dsvHeap)));

    auto desc = ds->GetDesc();
    depth.res = ds;
    depth.format = desc.Format;
    }

  D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = rtvHeap->GetCPUDescriptorHandleForHeapStart();
  rtvHeapInc = device.GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
  for(size_t i=0;i<viewsCount;++i) {
    device.CreateRenderTargetView(views[i].res, nullptr, rtvHandle);
    rtvHandle.ptr+=rtvHeapInc;
    }

  if(ds!=nullptr) {
    D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dsvHeap->GetCPUDescriptorHandleForHeapStart();
    device.CreateDepthStencilView(depth.res, nullptr, dsvHandle);
    }
  }

#endif
