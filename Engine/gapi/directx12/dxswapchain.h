#pragma once

#include <Tempest/AbstractGraphicsApi>

#include <dxgi1_6.h>
#include <d3d12.h>

#include "dxfence.h"
#include "gapi/directx12/comptr.h"

namespace Tempest {

class Semaphore;

namespace Detail {

class DxDevice;

class DxSwapchain : public AbstractGraphicsApi::Swapchain {
  public:
    DxSwapchain(DxDevice& device, IDXGIFactory4& dxgi, SystemApi::Window* hwnd);
    DxSwapchain(DxSwapchain&& other) = delete;
    ~DxSwapchain() override;

    uint32_t                 w()      const override { return imgW; }
    uint32_t                 h()      const override { return imgH; }

    void                     reset() override;
    uint32_t                 imageCount() const override { return imgCount; }
    uint32_t                 currentBackBufferIndex() override;

    void                     queuePresent();

    DXGI_FORMAT              format() const { return frm; }

    ComPtr<IDXGISwapChain3>                        impl;
    std::unique_ptr<ComPtr<ID3D12Resource>[]>      views;
    ComPtr<ID3D12DescriptorHeap>                   rtvHeap;
    std::unique_ptr<D3D12_CPU_DESCRIPTOR_HANDLE[]> handles;

  private:
    void               initImages();

    DxDevice&          dev;
    DxFence            fence;

    uint32_t           currImg=0;
    uint32_t           imgW=0, imgH=0;
    uint32_t           imgCount=3;
    DXGI_FORMAT        frm = DXGI_FORMAT_B8G8R8A8_UNORM;

    UINT64             frameCounter = 0;

    ComPtr<IDXGISwapChain1> swapChain;
  };

}}
