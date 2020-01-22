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
  dxAssert(device->CreateCommandQueue(&queueDesc, uuid<ID3D12CommandQueue>(), reinterpret_cast<void**>(&cmdQueue.impl)));

  allocator.setDevice(*this);
  }

DxDevice::~DxDevice() {
  }

const char* Detail::DxDevice::renderer() const {
  return description;
  }

void Detail::DxDevice::waitIdle() const {
  // TODO
  }
/*
void DxDevice::createBuffer(size_t byteSize) {
  ID3D12Resource* defaultBuffer=nullptr;

  D3D12_HEAP_PROPERTIES heapProp={};
  heapProp.Type                 = D3D12_HEAP_TYPE_DEFAULT;
  heapProp.CPUPageProperty      = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
  heapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
  heapProp.CreationNodeMask     = 1;
  heapProp.VisibleNodeMask      = 1;

  D3D12_RESOURCE_DESC resDesc={};
  resDesc.Dimension          = D3D12_RESOURCE_DIMENSION_BUFFER;
  resDesc.Alignment          = 0;
  resDesc.Width              = byteSize;
  resDesc.Height             = 1;
  resDesc.DepthOrArraySize   = 1;
  resDesc.MipLevels          = 1;
  resDesc.Format             = DXGI_FORMAT_UNKNOWN;
  resDesc.SampleDesc.Count   = 1;
  resDesc.SampleDesc.Quality = 0;
  resDesc.Layout             = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
  resDesc.Flags              = D3D12_RESOURCE_FLAG_NONE;

  device->CreateCommittedResource(
        &heapProp,
        D3D12_HEAP_FLAG_NONE,
        &resDesc,
        D3D12_RESOURCE_STATE_COMMON,
        nullptr,
        __uuidof(ID3D12Resource),
        reinterpret_cast<void**>(&defaultBuffer)
        );
  device->CreateCommittedResource(
    &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
    D3D12_HEAP_FLAG_NONE,
    &CD3DX12_RESOURCE_DESC::Buffer(byteSize),
    D3D12_RESOURCE_STATE_GENERIC_READ,
    NULL,
    __uuidof(ID3D12Resource),
    reinterpret_cast<void**>(&uploadBuffer)
  );

  D3D12_SUBRESOURCE_DATA data = {};
  data.pData      = initData;
  data.RowPitch   = byteSize;
  data.SlicePitch = data.RowPitch;

  cmdLst->ResourceBarrier(
    1,
    &CD3DX12_RESOURCE_BARRIER::Transition(defaultBuffer, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST)
  );

  UpdateSubresources<1>(cmdLst, defaultBuffer, uploadBuffer, 0, 0, 1, &data);

  cmdLst->ResourceBarrier(
    1,
    &CD3DX12_RESOURCE_BARRIER::Transition(defaultBuffer, D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ)
  );

  return defaultBuffer;
  }*/

#endif
