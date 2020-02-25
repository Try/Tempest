#if defined(TEMPEST_BUILD_DIRECTX12)

#include "dxframebuffer.h"
#include "dxdevice.h"
#include "dxswapchain.h"
#include "guid.h"

using namespace Tempest;
using namespace Tempest::Detail;

DxFramebuffer::DxFramebuffer(DxDevice& dev, DxSwapchain& swapchain, size_t image)
  :viewsCount(1) {
  auto& device = *dev.device;

  // descriptor heap
  D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
  rtvHeapDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
  rtvHeapDesc.NumDescriptors = 1;
  rtvHeapDesc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
  dxAssert(device.CreateDescriptorHeap(&rtvHeapDesc, uuid<ID3D12DescriptorHeap>(), reinterpret_cast<void**>(&rtvHeap)));

  // frame resources.
  views.reset(new ComPtr<ID3D12Resource>[1]);
  D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = rtvHeap->GetCPUDescriptorHandleForHeapStart();
  //auto eltSize = device.GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

  dxAssert(swapchain.impl->GetBuffer(UINT(image), uuid<ID3D12Resource>(), reinterpret_cast<void**>(&views[0])));
  device.CreateRenderTargetView(views[0].get(), nullptr, rtvHandle);
  }

DxFramebuffer::~DxFramebuffer() {
  }

#endif
