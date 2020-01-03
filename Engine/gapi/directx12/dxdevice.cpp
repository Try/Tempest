#if defined(_MSC_VER)

#include "guid.h"
#include "dxdevice.h"

using namespace Tempest;
using namespace Tempest::Detail;

DxDevice::DxDevice(IDXGIFactory4& dxgi) {
  ComPtr<IDXGIAdapter1> adapter;

  for(UINT i = 0; DXGI_ERROR_NOT_FOUND != dxgi.EnumAdapters1(i, &adapter.get()); ++i) {
    DXGI_ADAPTER_DESC1 desc={};
    adapter->GetDesc1(&desc);

    if(desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
      continue;

    // Check to see if the adapter supports Direct3D 12, but don't create the
    // actual device yet.
    if(SUCCEEDED(D3D12CreateDevice(adapter.get(), D3D_FEATURE_LEVEL_12_0, uuid<ID3D12Device>(), nullptr)))
      break;
    }

  dxAssert(D3D12CreateDevice(adapter.get(), D3D_FEATURE_LEVEL_12_0, uuid<ID3D12Device>(), reinterpret_cast<void**>(&device)));

  D3D12_COMMAND_QUEUE_DESC queueDesc = {};
  queueDesc.Type  = D3D12_COMMAND_LIST_TYPE_DIRECT;
  queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
  dxAssert(device->CreateCommandQueue(&queueDesc, __uuidof(ID3D12CommandQueue), reinterpret_cast<void**>(&cmdQueue)));
  }

DxDevice::~DxDevice() {
  }

const char* Detail::DxDevice::renderer() const {
  return "directx12";
  }

void Detail::DxDevice::waitIdle() const {
  // TODO
  }

#endif
