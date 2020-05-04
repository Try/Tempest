#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <stdexcept>
#include <mutex>

#include <dxgi1_6.h>
#include <d3d12.h>

#include "gapi/directx12/comptr.h"
#include "exceptions/exception.h"
#include "utility/spinlock.h"
#include "utility/compiller_hints.h"
#include "dxallocator.h"
#include "dxfence.h"
#include "gapi/uploadengine.h"

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

    DxDevice(IDXGIFactory4& dxgi);
    ~DxDevice() override;

    using DataMgr = UploadEngine<DxDevice,DxCommandBuffer,DxFence,DxBuffer,DxTexture>;
    using Data    = DataMgr::Data;

    void         waitData();
    const char*  renderer() const override;
    void         waitIdle() override;

    void         submit(DxCommandBuffer& cmd,DxFence& sync);

    AbstractGraphicsApi::Props props;
    ComPtr<ID3D12Device>       device;
    ComPtr<ID3D12CommandQueue> cmdQueue;

    DxAllocator                allocator;

  private:
    char                       description[128] = {};

    ComPtr<ID3D12Fence>        idleFence;
    HANDLE                     idleEvent=nullptr;

    std::unique_ptr<DataMgr>   data;

  friend class DataMgr::Data;
  };

}}
