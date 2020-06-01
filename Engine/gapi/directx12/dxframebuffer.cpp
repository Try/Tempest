#if defined(TEMPEST_BUILD_DIRECTX12)

#include "dxframebuffer.h"
#include "dxdevice.h"
#include "dxswapchain.h"
#include "dxtexture.h"
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
  views.reset(new ID3D12Resource*[viewsCount]);
  views[0] = swapchain.views[image].get();

  D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = rtvHeap->GetCPUDescriptorHandleForHeapStart();
  rtvHeapInc = device.GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
  device.CreateRenderTargetView(views[0], nullptr, rtvHandle);
  }

DxFramebuffer::DxFramebuffer(DxDevice& dev, DxSwapchain& swapchain, size_t image, DxTexture& zbuf)
  :viewsCount(2) {
  auto& device = *dev.device;

  // descriptor heap
  D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
  rtvHeapDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
  rtvHeapDesc.NumDescriptors = 2;
  rtvHeapDesc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
  dxAssert(device.CreateDescriptorHeap(&rtvHeapDesc, uuid<ID3D12DescriptorHeap>(), reinterpret_cast<void**>(&rtvHeap)));

  // frame resources.
  views.reset(new ID3D12Resource*[viewsCount]);
  views[0] = swapchain.views[image].get();
  views[1] = zbuf.impl.get();

  D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = rtvHeap->GetCPUDescriptorHandleForHeapStart();
  rtvHeapInc = device.GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
  for(size_t i=0;i<viewsCount;++i) {
    device.CreateRenderTargetView(views[i], nullptr, rtvHandle);
    rtvHandle.ptr+=rtvHeapInc;
    }
  }

DxFramebuffer::DxFramebuffer(DxDevice& dev, DxTexture& cl, DxTexture& zbuf)
  :viewsCount(2) {
  auto& device = *dev.device;

  // descriptor heap
  D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
  rtvHeapDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
  rtvHeapDesc.NumDescriptors = 2;
  rtvHeapDesc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
  dxAssert(device.CreateDescriptorHeap(&rtvHeapDesc, uuid<ID3D12DescriptorHeap>(), reinterpret_cast<void**>(&rtvHeap)));

  // frame resources.
  views.reset(new ID3D12Resource*[viewsCount]);
  views[0] = cl.impl.get();
  views[1] = zbuf.impl.get();

  D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = rtvHeap->GetCPUDescriptorHandleForHeapStart();
  rtvHeapInc = device.GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
  for(size_t i=0;i<viewsCount;++i) {
    device.CreateRenderTargetView(views[i], nullptr, rtvHandle);
    rtvHandle.ptr+=rtvHeapInc;
    }
  }

DxFramebuffer::DxFramebuffer(DxDevice& dev, DxTexture& cl)
  :viewsCount(1) {
  auto& device = *dev.device;
  // descriptor heap
  D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
  rtvHeapDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
  rtvHeapDesc.NumDescriptors = 1;
  rtvHeapDesc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
  dxAssert(device.CreateDescriptorHeap(&rtvHeapDesc, uuid<ID3D12DescriptorHeap>(), reinterpret_cast<void**>(&rtvHeap)));

  // frame resources.
  views.reset(new ID3D12Resource*[1]);
  views[0] = cl.impl.get();

  D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = rtvHeap->GetCPUDescriptorHandleForHeapStart();
  rtvHeapInc = device.GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
  device.CreateRenderTargetView(views[0], nullptr, rtvHandle);
  }

DxFramebuffer::~DxFramebuffer() {
  }

#endif
