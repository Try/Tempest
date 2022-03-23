#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <Tempest/RenderState>
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

class DxCompPipeline;

inline void dxAssert(HRESULT code){
  if(T_LIKELY(code==S_OK))
    return;

  switch( code ) {
    case D3D11_ERROR_TOO_MANY_UNIQUE_STATE_OBJECTS:
    case D3D11_ERROR_TOO_MANY_UNIQUE_VIEW_OBJECTS:
      throw std::system_error(Tempest::GraphicsErrc::OutOfVideoMemory);
    case E_OUTOFMEMORY:
      throw std::bad_alloc();
    case DXGI_ERROR_DRIVER_INTERNAL_ERROR:
    case DXGI_DDI_ERR_WASSTILLDRAWING:
    case DXGI_ERROR_DEVICE_REMOVED:
    case DXGI_ERROR_DEVICE_RESET:
      throw DeviceLostException();

    case DXGI_ERROR_ACCESS_DENIED:
    case DXGI_ERROR_ACCESS_LOST:
      throw DeviceLostException();

    case DXGI_ERROR_DEVICE_HUNG:
      throw DeviceHangException();

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

inline D3D12_BLEND nativeFormat(RenderState::BlendMode b) {
  switch(b) {
    case RenderState::BlendMode::Zero:             return D3D12_BLEND_ZERO;
    case RenderState::BlendMode::One:              return D3D12_BLEND_ONE;
    case RenderState::BlendMode::SrcColor:         return D3D12_BLEND_SRC_COLOR;
    case RenderState::BlendMode::OneMinusSrcColor: return D3D12_BLEND_INV_SRC_COLOR;
    case RenderState::BlendMode::SrcAlpha:         return D3D12_BLEND_SRC_ALPHA;
    case RenderState::BlendMode::OneMinusSrcAlpha: return D3D12_BLEND_INV_SRC_ALPHA;
    case RenderState::BlendMode::DstAlpha:         return D3D12_BLEND_DEST_ALPHA;
    case RenderState::BlendMode::OneMinusDstAlpha: return D3D12_BLEND_INV_DEST_ALPHA;
    case RenderState::BlendMode::DstColor:         return D3D12_BLEND_DEST_COLOR;
    case RenderState::BlendMode::OneMinusDstColor: return D3D12_BLEND_INV_DEST_COLOR;
    case RenderState::BlendMode::SrcAlphaSaturate: return D3D12_BLEND_SRC_ALPHA_SAT;
    }
  return D3D12_BLEND_ZERO;
  }

inline D3D12_BLEND_OP nativeFormat(RenderState::BlendOp op) {
  switch(op) {
    case RenderState::BlendOp::Add:             return D3D12_BLEND_OP_ADD;
    case RenderState::BlendOp::Subtract:        return D3D12_BLEND_OP_SUBTRACT;
    case RenderState::BlendOp::ReverseSubtract: return D3D12_BLEND_OP_REV_SUBTRACT;
    case RenderState::BlendOp::Min:             return D3D12_BLEND_OP_MIN;
    case RenderState::BlendOp::Max:             return D3D12_BLEND_OP_MAX;
    }
  return D3D12_BLEND_OP_ADD;
  }

inline D3D12_CULL_MODE nativeFormat(RenderState::CullMode c) {
  switch(c) {
    case RenderState::CullMode::Back:   return D3D12_CULL_MODE_BACK;
    case RenderState::CullMode::Front:  return D3D12_CULL_MODE_FRONT;
    case RenderState::CullMode::NoCull: return D3D12_CULL_MODE_NONE;
    }
  return D3D12_CULL_MODE_NONE;
  }

inline D3D12_COMPARISON_FUNC nativeFormat(RenderState::ZTestMode zm) {
  switch(zm) {
    case RenderState::ZTestMode::Always:  return D3D12_COMPARISON_FUNC_ALWAYS;
    case RenderState::ZTestMode::Never:   return D3D12_COMPARISON_FUNC_NEVER;
    case RenderState::ZTestMode::Greater: return D3D12_COMPARISON_FUNC_GREATER;
    case RenderState::ZTestMode::Less:    return D3D12_COMPARISON_FUNC_LESS;
    case RenderState::ZTestMode::GEqual:  return D3D12_COMPARISON_FUNC_GREATER_EQUAL;
    case RenderState::ZTestMode::LEqual:  return D3D12_COMPARISON_FUNC_LESS_EQUAL;
    case RenderState::ZTestMode::NOEqual: return D3D12_COMPARISON_FUNC_NOT_EQUAL;
    case RenderState::ZTestMode::Equal:   return D3D12_COMPARISON_FUNC_EQUAL;
    }
  return D3D12_COMPARISON_FUNC_ALWAYS;
  }

inline DXGI_FORMAT nativeFormat(Decl::ComponentType t) {
  switch(t) {
    case Decl::float0:
    case Decl::count:
      return DXGI_FORMAT_UNKNOWN;
    case Decl::float1:
      return DXGI_FORMAT_R32_FLOAT;
    case Decl::float2:
      return DXGI_FORMAT_R32G32_FLOAT;
    case Decl::float3:
      return DXGI_FORMAT_R32G32B32_FLOAT;
    case Decl::float4:
      return DXGI_FORMAT_R32G32B32A32_FLOAT;
    case Decl::int1:
      return DXGI_FORMAT_R32_SINT;
    case Decl::int2:
      return DXGI_FORMAT_R32G32_SINT;
    case Decl::int3:
      return DXGI_FORMAT_R32G32B32_SINT;
    case Decl::int4:
      return DXGI_FORMAT_R32G32B32A32_SINT;
    case Decl::uint1:
      return DXGI_FORMAT_R32_UINT;
    case Decl::uint2:
      return DXGI_FORMAT_R32G32_UINT;
    case Decl::uint3:
      return DXGI_FORMAT_R32G32B32_UINT;
    case Decl::uint4:
      return DXGI_FORMAT_R32G32B32A32_UINT;
    }
  return DXGI_FORMAT_UNKNOWN;
  }

inline UINT alignTo(UINT x, UINT a) {
  a--;
  return (x+a) & (~a);
  }

class DxPipeline;

class DxDevice : public AbstractGraphicsApi::Device {
  public:
    struct ApiEntry {
      HRESULT (WINAPI *D3D12CreateDevice)(IUnknown* pAdapter,
                                          D3D_FEATURE_LEVEL MinimumFeatureLevel,
                                          REFIID riid, // Expected: ID3D12Device
                                          void** ppDevice ) = nullptr;
      HRESULT (WINAPI *D3D12GetDebugInterface)(REFIID riid, void** ppvDebug ) = nullptr;
      HRESULT (WINAPI *D3D12SerializeRootSignature)(const D3D12_ROOT_SIGNATURE_DESC* pRootSignature,
                                                    D3D_ROOT_SIGNATURE_VERSION Version,
                                                    ID3DBlob** ppBlob,
                                                    ID3DBlob** ppErrorBlob) = nullptr;
      };

    DxDevice(IDXGIAdapter1& adapter, const ApiEntry& dllApi);
    ~DxDevice() override;

    using DataMgr = UploadEngine<DxDevice,DxCommandBuffer,DxFence,DxBuffer>;

    void         waitData();
    void         waitIdle() override;

    static void  getProp(IDXGIAdapter1& adapter, AbstractGraphicsApi::Props& prop);
    static void  getProp(DXGI_ADAPTER_DESC1& desc, AbstractGraphicsApi::Props& prop);
    void         submit(DxCommandBuffer& cmd,DxFence& sync);

    DataMgr&     dataMgr() { return *data; }

    ApiEntry                    dllApi;

    AbstractGraphicsApi::Props  props;
    ComPtr<ID3D12Device>        device;
    SpinLock                    syncCmdQueue;
    ComPtr<ID3D12CommandQueue>  cmdQueue;

    DxAllocator                 allocator;

    DSharedPtr<DxPipelineLay*>  blitLayout;
    DSharedPtr<DxPipeline*>     blit;

    DSharedPtr<DxPipelineLay*>  copyLayout;
    DSharedPtr<DxCompPipeline*> copy;
    DSharedPtr<DxCompPipeline*> copyS;

  private:
    ComPtr<ID3D12Fence>         idleFence;
    HANDLE                      idleEvent=nullptr;

    std::unique_ptr<DataMgr>    data;
  };

}}
