#if defined(_MSC_VER)

#include "dxswapchain.h"

using namespace Tempest;
using namespace Tempest::Detail;

DxSwapchain::DxSwapchain(DxDevice& device,uint32_t w,uint32_t h)
  :imgW(w), imgH(h) {
  DXGI_SWAP_CHAIN_DESC sd = {};
  sd.BufferDesc.Width  = w;
  sd.BufferDesc.Height = h;
  sd.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;

  sd.SampleDesc.Count = 1;
/*
  sd.BufferUsage  = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  sd.BufferCount  = imgCount;
  sd.OutputWindow = HWND(m_WindowHandle);
  sd.Windowed     = true;
  sd.SwapEffect   = DXGI_SWAP_EFFECT_FLIP_DISCARD;
  sd.Flags        = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

  dxAssert(m_DXGIFactory->CreateSwapChain(m_CmdQueue.Get(), &sd, m_SwapChain.GetAddressOf()));
  */
  }

DxSwapchain::~DxSwapchain() {
  }

uint32_t DxSwapchain::nextImage(AbstractGraphicsApi::Semaphore* onReady) {
  return currImg%imgCount;
  }

AbstractGraphicsApi::Image* DxSwapchain::getImage(uint32_t id) {
  return nullptr;
  }

#endif
