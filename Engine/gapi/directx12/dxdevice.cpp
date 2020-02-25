#if defined(TEMPEST_BUILD_DIRECTX12)

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

  DXGI_ADAPTER_DESC desc={};
  adapter->GetDesc(&desc);
  for(size_t i=0;i<sizeof(description);++i)  {
    WCHAR c = desc.Description[i];
    if(c==0)
      break;
    if(('0'<=c && c<='9') || ('a'<=c && c<='z') || ('A'<=c && c<='Z') ||
       c=='(' || c==')' || c=='_' || c=='[' || c==']' || c=='{' || c=='}' || c==' ')
      description[i] = char(c); else
      description[i] = '?';
    }
  description[sizeof(description)-1]='\0';

  D3D12_COMMAND_QUEUE_DESC queueDesc = {};
  queueDesc.Type  = D3D12_COMMAND_LIST_TYPE_DIRECT;
  queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
  dxAssert(device->CreateCommandQueue(&queueDesc, uuid<ID3D12CommandQueue>(), reinterpret_cast<void**>(&cmdQueue)));

  dxAssert(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, uuid<ID3D12CommandAllocator>(), reinterpret_cast<void**>(&cmdMain)));

  allocator.setDevice(*this);

  dxAssert(device->CreateFence(DxFence::Waiting, D3D12_FENCE_FLAG_NONE,
                               uuid<ID3D12Fence>(),
                               reinterpret_cast<void**>(&idleFence)));
  idleEvent = CreateEvent(nullptr, TRUE, TRUE, nullptr);
  }

DxDevice::~DxDevice() {
  CloseHandle(idleEvent);
  }

const char* Detail::DxDevice::renderer() const {
  return description;
  }

void Detail::DxDevice::waitIdle() {
  idleFence->Signal(DxFence::Ready);
  cmdQueue->Wait(idleFence.get(),DxFence::Ready);
  idleFence->Signal(DxFence::Waiting);
  ResetEvent(idleEvent);
  }

#endif
