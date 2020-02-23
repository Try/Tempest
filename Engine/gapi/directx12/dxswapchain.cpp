#if defined(TEMPEST_BUILD_DIRECTX12)

#include "dxswapchain.h"
#include "dxdevice.h"

using namespace Tempest;
using namespace Tempest::Detail;

DxSwapchain::DxSwapchain(DxDevice& device, IDXGIFactory4& dxgi, SystemApi::Window* hwnd) {
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
  sd.SwapEffect   = DXGI_SWAP_EFFECT_FLIP_DISCARD;
  sd.Scaling      = DXGI_SCALING_STRETCH;
  sd.SwapEffect   = DXGI_SWAP_EFFECT_DISCARD;
  sd.AlphaMode    = DXGI_ALPHA_MODE_UNSPECIFIED;
/*
  sd.Flags        = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
  */

  ComPtr<IDXGISwapChain1> swapChain;
  dxAssert(dxgi.CreateSwapChainForHwnd(
      device.cmdQueue.get(),        // Swap chain needs the queue so that it can force a flush on it.
      HWND(hwnd),
      &sd,
      nullptr,
      nullptr,
      &swapChain.get()
      ));


  dxAssert(dxgi.MakeWindowAssociation(HWND(hwnd), DXGI_MWA_NO_ALT_ENTER));
  dxAssert(swapChain->QueryInterface(__uuidof(IDXGISwapChain3), reinterpret_cast<void**>(&impl)));
  /*
  // Create frame resources.
  {
      CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart());

      // Create a RTV for each frame.
      for (UINT n = 0; n < FrameCount; n++)
      {
          ThrowIfFailed(m_swapChain->GetBuffer(n, IID_PPV_ARGS(&m_renderTargets[n])));
          m_device->CreateRenderTargetView(m_renderTargets[n].Get(), nullptr, rtvHandle);
          rtvHandle.Offset(1, m_rtvDescriptorSize);
      }
  }*/
  }

DxSwapchain::~DxSwapchain() {
  }

uint32_t DxSwapchain::nextImage(AbstractGraphicsApi::Semaphore* onReady) {
  return impl->GetCurrentBackBufferIndex();
  }

#endif
