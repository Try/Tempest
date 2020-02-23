#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <stdexcept>

#include <dxgi1_6.h>
#include <d3d12.h>

#include "gapi/directx12/comptr.h"
#include "exceptions/exception.h"
#include "utility/spinlock.h"
#include "utility/compiller_hints.h"
#include "dxallocator.h"

namespace Tempest {

class DirectX12Api;

namespace Detail {

inline void dxAssert(HRESULT code){
  if(T_LIKELY(code==S_OK))
    return;

  switch( code ) {
    case D3D11_ERROR_TOO_MANY_UNIQUE_STATE_OBJECTS:
    case D3D11_ERROR_TOO_MANY_UNIQUE_VIEW_OBJECTS:
      throw std::system_error(Tempest::GraphicsErrc::OutOfVideoMemory);
    case E_OUTOFMEMORY:
      throw std::bad_alloc();
    case DXGI_DDI_ERR_WASSTILLDRAWING:
      throw DeviceLostException();

    case E_NOTIMPL:
      throw std::runtime_error("engine internal error");

    default:
      throw std::runtime_error("undefined directx error"); //TODO
    }
  }

class DxDevice : public AbstractGraphicsApi::Device {
  public:
    using ResPtr = Detail::DSharedPtr<AbstractGraphicsApi::Shared*>;
    //using BufPtr = Detail::DSharedPtr<DxBuffer*>;
    //using TexPtr = Detail::DSharedPtr<DxTexture*>;

    DxDevice(IDXGIFactory4& dxgi);
    ~DxDevice() override;

    AbstractGraphicsApi::Props props;

    const char*             renderer() const override;
    void                    waitIdle() const override;

    struct Data {

      };

    ComPtr<ID3D12Device>           device;
    ComPtr<ID3D12CommandAllocator> cmdMain;
    ComPtr<ID3D12CommandQueue>     cmdQueue;

    DxAllocator                    allocator;

  private:
    char                           description[128] = {};
  };

}}
