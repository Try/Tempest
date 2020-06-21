#if defined(TEMPEST_BUILD_DIRECTX12)

#include <Tempest/Log>

#include "guid.h"
#include "dxdevice.h"

#include "dxbuffer.h"
#include "dxtexture.h"

using namespace Tempest;
using namespace Tempest::Detail;

DxDevice::DxDevice(IDXGIAdapter1& adapter) {
  dxAssert(D3D12CreateDevice(&adapter, D3D_FEATURE_LEVEL_12_0, uuid<ID3D12Device>(), reinterpret_cast<void**>(&device)));

  ComPtr<ID3D12InfoQueue> pInfoQueue;
  if(SUCCEEDED(device->QueryInterface(__uuidof(ID3D12InfoQueue),reinterpret_cast<void**>(&pInfoQueue)))) {
    // Suppress messages based on their severity level
    D3D12_MESSAGE_SEVERITY severities[] = {
      D3D12_MESSAGE_SEVERITY_INFO
      };
    D3D12_MESSAGE_ID denyIds[] = {
      // I'm really not sure how to avoid this message.
      D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE,
      D3D12_MESSAGE_ID_CLEARDEPTHSTENCILVIEW_MISMATCHINGCLEARVALUE
      };
    D3D12_INFO_QUEUE_FILTER filter = {};
    filter.DenyList.NumSeverities = _countof(severities);
    filter.DenyList.pSeverityList = severities;
    filter.DenyList.NumIDs        = _countof(denyIds);
    filter.DenyList.pIDList       = denyIds;

    dxAssert(pInfoQueue->PushStorageFilter(&filter));
    }

  DXGI_ADAPTER_DESC desc={};
  adapter.GetDesc(&desc);
  getProp(adapter,props);

  D3D12_COMMAND_QUEUE_DESC queueDesc = {};
  queueDesc.Type  = D3D12_COMMAND_LIST_TYPE_DIRECT;
  queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
  dxAssert(device->CreateCommandQueue(&queueDesc, uuid<ID3D12CommandQueue>(), reinterpret_cast<void**>(&cmdQueue)));

  allocator.setDevice(*this);

  dxAssert(device->CreateFence(DxFence::Waiting, D3D12_FENCE_FLAG_NONE,
                               uuid<ID3D12Fence>(),
                               reinterpret_cast<void**>(&idleFence)));
  idleEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);

  data.reset(new DataMgr(*this));
  }

DxDevice::~DxDevice() {
  data.reset();
  CloseHandle(idleEvent);
  }

void DxDevice::getProp(IDXGIAdapter1& adapter, AbstractGraphicsApi::Props& prop) {
  DXGI_ADAPTER_DESC1 desc={};
  adapter.GetDesc1(&desc);
  return getProp(desc,prop);
  }

void DxDevice::getProp(DXGI_ADAPTER_DESC1& desc, AbstractGraphicsApi::Props& prop) {
  for(size_t i=0;i<sizeof(prop.name);++i)  {
    WCHAR c = desc.Description[i];
    if(c==0)
      break;
    if(('0'<=c && c<='9') || ('a'<=c && c<='z') || ('A'<=c && c<='Z') ||
       c=='(' || c==')' || c=='_' || c=='[' || c==']' || c=='{' || c=='}' || c==' ')
      prop.name[i] = char(c); else
      prop.name[i] = '?';
    }
  prop.name[sizeof(prop.name)-1]='\0';

  // https://docs.microsoft.com/en-us/windows/win32/direct3ddxgi/hardware-support-for-direct3d-12-0-formats
  static const TextureFormat smp[] = {TextureFormat::R8,   TextureFormat::RG8,  TextureFormat::RGBA8,
                                      TextureFormat::R16,  TextureFormat::RG16, TextureFormat::RGBA16,
                                      TextureFormat::DXT1, TextureFormat::DXT3, TextureFormat::DXT5};
  static const TextureFormat att[] = {TextureFormat::R8,   TextureFormat::RG8,  TextureFormat::RGBA8,
                                      TextureFormat::R16,  TextureFormat::RG16, TextureFormat::RGBA16};
  static const TextureFormat ds[]  = {TextureFormat::Depth16, TextureFormat::Depth24x8, TextureFormat::Depth24S8};
  uint64_t smpBit = 0, attBit = 0, dsBit = 0;
  for(auto& i:smp)
    smpBit |= uint64_t(1) << uint64_t(i);
  for(auto& i:att)
    attBit |= uint64_t(1) << uint64_t(i);
  for(auto& i:ds)
    dsBit  |= uint64_t(1) << uint64_t(i);
  prop.setSamplerFormats(smpBit);
  prop.setAttachFormats (attBit);
  prop.setDepthFormat   (dsBit);

  // TODO: buffer limitsk
  prop.ubo.maxRange;
  prop.ubo.offsetAlign = 256;

  prop.anisotropy    = true;
  prop.maxAnisotropy = 16;
  }

void DxDevice::waitData() {
  data->wait();
  }

const char* Detail::DxDevice::renderer() const {
  return props.name;
  }

void Detail::DxDevice::waitIdle() {
  dxAssert(cmdQueue->Signal(idleFence.get(),DxFence::Ready));
  dxAssert(idleFence->SetEventOnCompletion(DxFence::Ready,idleEvent));
  WaitForSingleObjectEx(idleEvent, INFINITE, FALSE);
  dxAssert(idleFence->Signal(DxFence::Waiting));
  }

void DxDevice::submit(DxCommandBuffer& cmdBuffer, DxFence& sync) {
  sync.reset();

  ID3D12CommandList* cmd[] = {cmdBuffer.get()};
  cmdQueue->ExecuteCommandLists(1, cmd);
  sync.signal(*cmdQueue);
  }

#endif

