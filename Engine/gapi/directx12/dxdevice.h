#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <stdexcept>
#include <mutex>

#include <dxgi1_6.h>
#include <d3d12.h>

#include "gapi/directx12/comptr.h"
#include "gapi/directx12/dxcommandbuffer.h"
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
    case DXGI_ERROR_DEVICE_REMOVED:
      throw DeviceLostException();

    case E_NOTIMPL:
    case E_INVALIDARG:
      throw std::runtime_error("engine internal error");

    default:
      throw std::runtime_error("undefined directx error"); //TODO
    }
  }

inline DXGI_FORMAT nativeFormat(TextureFormat f) {
  static const DXGI_FORMAT vfrm[]={
    DXGI_FORMAT_UNKNOWN,
    DXGI_FORMAT_R8_UNORM,
    DXGI_FORMAT_R8G8_UNORM,
    DXGI_FORMAT_UNKNOWN, // DXGI_FORMAT_R8G8B8_UNORM ?
    DXGI_FORMAT_R8G8B8A8_UNORM,
    DXGI_FORMAT_R16_UNORM,
    DXGI_FORMAT_R16G16_UNORM,
    DXGI_FORMAT_UNKNOWN, // DXGI_FORMAT_R16G16B16_UNORM ?
    DXGI_FORMAT_R16G16B16A16_UNORM,
    DXGI_FORMAT_R32_FLOAT,
    DXGI_FORMAT_R32G32_FLOAT,
    DXGI_FORMAT_R32G32B32_FLOAT,
    DXGI_FORMAT_R32G32B32A32_FLOAT,
    DXGI_FORMAT_D16_UNORM,
    DXGI_FORMAT_D24_UNORM_S8_UINT,
    DXGI_FORMAT_D24_UNORM_S8_UINT,
    DXGI_FORMAT_BC1_UNORM,
    DXGI_FORMAT_BC2_UNORM,
    DXGI_FORMAT_BC3_UNORM,
    };
  return vfrm[f];
  }

inline D3D12_RESOURCE_STATES nativeFormat(TextureLayout f) {
  static const D3D12_RESOURCE_STATES lay[]={
    D3D12_RESOURCE_STATE_COMMON,
    D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE|D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
    D3D12_RESOURCE_STATE_RENDER_TARGET,
    D3D12_RESOURCE_STATE_DEPTH_WRITE,
    D3D12_RESOURCE_STATE_PRESENT,
    D3D12_RESOURCE_STATE_COPY_SOURCE,
    D3D12_RESOURCE_STATE_COPY_DEST,
    D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
    };
  return lay[uint8_t(f)];
  }

inline D3D12_TEXTURE_ADDRESS_MODE nativeFormat(ClampMode f){
  static const D3D12_TEXTURE_ADDRESS_MODE vfrm[]={
    D3D12_TEXTURE_ADDRESS_MODE_BORDER,
    D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
    D3D12_TEXTURE_ADDRESS_MODE_MIRROR,
    D3D12_TEXTURE_ADDRESS_MODE_WRAP,
    D3D12_TEXTURE_ADDRESS_MODE_WRAP,
    };
  return vfrm[int(f)];
  }

inline bool nativeIsDepthFormat(DXGI_FORMAT frm) {
  if(frm==DXGI_FORMAT_D32_FLOAT_S8X24_UINT ||
     frm==DXGI_FORMAT_D32_FLOAT ||
     frm==DXGI_FORMAT_D24_UNORM_S8_UINT ||
     frm==DXGI_FORMAT_D16_UNORM)
    return true;
  return false;
  }

inline UINT alignTo(UINT x, UINT a) {
  a--;
  return (x+a) & (~a);
  }

class DxCompPipeline;

class DxDevice : public AbstractGraphicsApi::Device {
  public:
    DxDevice(IDXGIAdapter1& adapter);
    ~DxDevice() override;

    using DataMgr = UploadEngine<DxDevice,DxCommandBuffer,DxFence>;

    void         waitData();
    const char*  renderer() const override;
    void         waitIdle() override;

    static void  getProp(IDXGIAdapter1& adapter, AbstractGraphicsApi::Props& prop);
    static void  getProp(DXGI_ADAPTER_DESC1& desc, AbstractGraphicsApi::Props& prop);
    void         submit(DxCommandBuffer& cmd,DxFence& sync);

    DataMgr&     dataMgr() { return *data; }

    AbstractGraphicsApi::Props  props;
    ComPtr<ID3D12Device>        device;
    SpinLock                    syncCmdQueue;
    ComPtr<ID3D12CommandQueue>  cmdQueue;

    DxAllocator                 allocator;

    DSharedPtr<DxUniformsLay*>  blitLayout;
    DSharedPtr<DxCompPipeline*> blit;

  private:
    ComPtr<ID3D12Fence>         idleFence;
    HANDLE                      idleEvent=nullptr;

    std::unique_ptr<DataMgr>    data;
  };

}}
