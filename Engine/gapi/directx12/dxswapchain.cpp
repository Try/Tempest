#if defined(TEMPEST_BUILD_DIRECTX12)

#include "dxswapchain.h"
#include "dxdevice.h"
#include "guid.h"

using namespace Tempest;
using namespace Tempest::Detail;

DxSwapchain::DxSwapchain(DxDevice& dev, IDXGIFactory4& dxgi, SystemApi::Window* hwnd)
  :dev(dev) {
  auto& device = *dev.device;

  dxAssert(device.CreateFence(0, D3D12_FENCE_FLAG_NONE, uuid<ID3D12Fence>(), reinterpret_cast<void**>(&fence)));
  idleEvent = DxEvent(false);

  auto rect = SystemApi::windowClientRect(hwnd);
  imgW = uint32_t(rect.w);
  imgH = uint32_t(rect.h);
  imgCount = 3;// TODO

  DXGI_SWAP_CHAIN_DESC1 sd = {};
  sd.Width  = imgW;
  sd.Height = imgH;
  sd.Format = frm;
  sd.Stereo = false;

  sd.SampleDesc.Count = 1;
  sd.BufferUsage  = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  sd.BufferCount  = imgCount;
  sd.Scaling      = DXGI_SCALING_STRETCH;
  sd.SwapEffect   = DXGI_SWAP_EFFECT_FLIP_DISCARD;
  sd.AlphaMode    = DXGI_ALPHA_MODE_UNSPECIFIED;
  /*
  sd.Flags        = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
  */

  dxAssert(dxgi.CreateSwapChainForHwnd(
      dev.cmdQueue.get(),        // Swap chain needs the queue so that it can force a flush on it.
      HWND(hwnd),
      &sd,
      nullptr,
      nullptr,
      &swapChain.get()
      ));

  dxAssert(dxgi.MakeWindowAssociation(HWND(hwnd), DXGI_MWA_NO_ALT_ENTER));
  dxAssert(swapChain->QueryInterface(uuid<IDXGISwapChain3>(), reinterpret_cast<void**>(&impl)));

  rtvHeap = dev.descAlloc.allocRtv(imgCount);
  views.reset(new ComPtr<ID3D12Resource>[imgCount]);

  initImages();
  }

DxSwapchain::~DxSwapchain() {
  dev.descAlloc.free(rtvHeap);
  }

void DxSwapchain::reset() {
  //wait for all pending frame to be finizhed
  if(frameCounter>0) {
    std::lock_guard<SpinLock> guard(dev.syncCmdQueue);
    dxAssert(dev.cmdQueue->Signal(fence.get(), frameCounter), dev);
    dxAssert(fence->SetEventOnCompletion(frameCounter, idleEvent.hevt), dev);
    dxAssert(WaitForSingleObjectEx(idleEvent.hevt, INFINITE, FALSE), dev);
    ResetEvent(idleEvent.hevt);
    }

  for(uint32_t i=0; i<imgCount; ++i) {
    views[i] = nullptr;
    }

  HWND hwnd={};
  dxAssert(impl->GetHwnd(&hwnd));
  auto rect = SystemApi::windowClientRect(reinterpret_cast<SystemApi::Window*>(hwnd));
  imgW = uint32_t(rect.w);
  imgH = uint32_t(rect.h);

  dxAssert(impl->ResizeBuffers(imgCount, imgW, imgH, frm, 0));
  initImages();
  }

uint32_t DxSwapchain::currentBackBufferIndex() {
  return impl->GetCurrentBackBufferIndex();
  }

void DxSwapchain::queuePresent() {
  static int vsunc = 1;
  dxAssert(impl->Present(vsunc, 0));

  ++frameCounter;
  dev.cmdQueue->Signal(fence.get(), frameCounter);
  }

void DxSwapchain::initImages() {
  auto& device  = *dev.device;
  auto  rtvSize = device.GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

  D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = dev.descAlloc.handle(rtvHeap);
  for(uint32_t i=0; i<imgCount; i++) {
    dxAssert(swapChain->GetBuffer(i, uuid<ID3D12Resource>(), reinterpret_cast<void**>(&views[i])));
    device.CreateRenderTargetView(views[i].get(), nullptr, rtvHandle);
    rtvHandle.ptr += rtvSize;
    }
  }

D3D12_CPU_DESCRIPTOR_HANDLE DxSwapchain::handle(size_t i) const {
  return dev.descAlloc.handle(rtvHeap, i);
  }

#endif
