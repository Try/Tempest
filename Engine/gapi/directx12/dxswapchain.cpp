#if defined(TEMPEST_BUILD_DIRECTX12)

#include "dxswapchain.h"
#include "dxdevice.h"
#include "guid.h"

using namespace Tempest;
using namespace Tempest::Detail;

DxSwapchain::DxSwapchain(DxDevice& dev, IDXGIFactory4& dxgi, SystemApi::Window* hwnd) {
  auto& device = *dev.device;

  auto rect = SystemApi::windowClientRect(hwnd);
  imgW = uint32_t(rect.w);
  imgH = uint32_t(rect.h);
  imgCount = 3;// TODO

  DXGI_SWAP_CHAIN_DESC1 sd = {};
  sd.Width  = imgW;
  sd.Height = imgH;
  sd.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
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

  ComPtr<IDXGISwapChain1> swapChain;
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

  // descriptor heap
  D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
  rtvHeapDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
  rtvHeapDesc.NumDescriptors = imgCount;
  rtvHeapDesc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
  dxAssert(device.CreateDescriptorHeap(&rtvHeapDesc, uuid<ID3D12DescriptorHeap>(), reinterpret_cast<void**>(&rtvHeap)));

  // frame resources.
  views.reset(new ComPtr<ID3D12Resource>[imgCount]);
  D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = rtvHeap->GetCPUDescriptorHandleForHeapStart();
  auto eltSize = device.GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

  for(uint32_t i=0; i<imgCount; i++) {
    dxAssert(swapChain->GetBuffer(i, uuid<ID3D12Resource>(), reinterpret_cast<void**>(&views[i])));
    device.CreateRenderTargetView(views[i].get(), nullptr, rtvHandle);
    rtvHandle.ptr += eltSize;
    }
  }

DxSwapchain::~DxSwapchain() {
  }

void DxSwapchain::reset() {
  // TODO
  }

uint32_t DxSwapchain::nextImage(AbstractGraphicsApi::Semaphore* onReady) {
  return impl->GetCurrentBackBufferIndex();
  }

#endif
