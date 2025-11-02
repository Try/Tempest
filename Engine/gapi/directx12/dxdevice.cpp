#if defined(TEMPEST_BUILD_DIRECTX12)

#include <Tempest/Log>

#include "guid.h"
#include "dxdevice.h"

#include "dxshader.h"
#include "dxpipeline.h"
#include "builtin_shader.h"

using namespace Tempest;
using namespace Tempest::Detail;

void Tempest::Detail::dxAssert(HRESULT code, DxDevice& dx) {
  if(code!=DXGI_ERROR_DEVICE_REMOVED && code!=DXGI_ERROR_DEVICE_RESET) {
    dxAssert(code);
    return;
    }

  ID3D12Device& device = *dx.device;

  ComPtr<ID3D12DeviceRemovedExtendedData> pDred;
  if(device.QueryInterface(uuid<ID3D12DeviceRemovedExtendedData>(), reinterpret_cast<void**>(pDred.get()))>=0) {
    D3D12_DRED_PAGE_FAULT_OUTPUT pageFault = {};
    if(pDred->GetPageFaultAllocationOutput(&pageFault)>=0) {
      char message[128] = {};
      std::snprintf(message, sizeof(message), "page fault at %llx", pageFault.PageFaultVA);
      throw DeviceLostException(message);
      }
    }

  throw DeviceLostException();
  }

DxDevice::DxDevice(IDXGIAdapter1& adapter, const ApiEntry& dllApi)
  :dllApi(dllApi) {
  dxAssert(dllApi.D3D12CreateDevice(&adapter, preferredFeatureLevel, uuid<ID3D12Device>(), reinterpret_cast<void**>(&device)));

  ComPtr<ID3D12InfoQueue> pInfoQueue;
  if(SUCCEEDED(device->QueryInterface(uuid<ID3D12InfoQueue>(),reinterpret_cast<void**>(&pInfoQueue)))) {
    // Suppress messages based on their severity level
    D3D12_MESSAGE_SEVERITY severities[] = {
      D3D12_MESSAGE_SEVERITY_INFO
      };
    D3D12_MESSAGE_ID denyIds[] = {
      // I'm really not sure how to avoid this message.
      D3D12_MESSAGE_ID_CLEARRENDERTARGETVIEW_MISMATCHINGCLEARVALUE,
      D3D12_MESSAGE_ID_CLEARDEPTHSTENCILVIEW_MISMATCHINGCLEARVALUE,
      // blit shader uses no vbo
      D3D12_MESSAGE_ID_CREATEINPUTLAYOUT_EMPTY_LAYOUT,
      // debug markers without pix
      D3D12_MESSAGE_ID_CORRUPTED_PARAMETER2,
      // staging buffer (vulkan style)
      D3D12_MESSAGE_ID_WRITE_COMBINE_PERFORMANCE_WARNING,
      // swapchain internal warning on W11/NVdida
      D3D12_MESSAGE_ID(1424),
      };
    D3D12_INFO_QUEUE_FILTER filter = {};
    filter.DenyList.NumSeverities = _countof(severities);
    filter.DenyList.pSeverityList = severities;
    filter.DenyList.NumIDs        = _countof(denyIds);
    filter.DenyList.pIDList       = denyIds;

    dxAssert(pInfoQueue->PushStorageFilter(&filter));

    pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, true);
    pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
    pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, true);
    }

  ComPtr<ID3D12InfoQueue1> pInfoQueue1;
  if(SUCCEEDED(device->QueryInterface(uuid<ID3D12InfoQueue1>(),reinterpret_cast<void**>(&pInfoQueue1)))) {
    // NOTE: not supported in DX yet
    pInfoQueue1->RegisterMessageCallback(DxDevice::debugReportCallback, D3D12_MESSAGE_CALLBACK_FLAG_NONE, nullptr, nullptr);
    }

  if(dllApi.D3D12GetDebugInterface!=nullptr) {
    ComPtr<ID3D12DeviceRemovedExtendedDataSettings> dredSettings;
    dllApi.D3D12GetDebugInterface(uuid<ID3D12DeviceRemovedExtendedDataSettings>(), reinterpret_cast<void**>(&dredSettings.get()));

    if(dredSettings.get()!=nullptr)
      dredSettings->SetPageFaultEnablement(D3D12_DRED_ENABLEMENT_FORCED_ON);
    }

  dxAssert(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, uuid<ID3D12Fence>(), reinterpret_cast<void**>(&cmdFence)));
  idleEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);

  DXGI_ADAPTER_DESC desc={};
  adapter.GetDesc(&desc);
  getProp(adapter,*device,props);

  D3D12_COMMAND_QUEUE_DESC queueDesc = {};
  queueDesc.Type  = D3D12_COMMAND_LIST_TYPE_DIRECT;
  queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
  dxAssert(device->CreateCommandQueue(&queueDesc, uuid<ID3D12CommandQueue>(), reinterpret_cast<void**>(&cmdQueue)));

  {
    D3D12_INDIRECT_ARGUMENT_DESC arg = {};
    D3D12_COMMAND_SIGNATURE_DESC desc = {};
    desc.pArgumentDescs   = &arg;

    desc.ByteStride       = sizeof(D3D12_DRAW_ARGUMENTS);
    desc.NumArgumentDescs = 1;
    arg.Type              = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW;
    dxAssert(device->CreateCommandSignature(&desc, nullptr, uuid<ID3D12CommandSignature>(), reinterpret_cast<void**>(&drawIndirectSgn)));

    if(props.meshlets.meshShader) {
      desc.ByteStride       = sizeof(D3D12_DISPATCH_MESH_ARGUMENTS);
      desc.NumArgumentDescs = 1;
      arg.Type              = D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH_MESH;
      dxAssert(device->CreateCommandSignature(&desc, nullptr, uuid<ID3D12CommandSignature>(), reinterpret_cast<void**>(&drawMeshIndirectSgn)));
      }

    arg.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH;
    desc.ByteStride = sizeof(D3D12_DISPATCH_ARGUMENTS);
    dxAssert(device->CreateCommandSignature(&desc, nullptr, uuid<ID3D12CommandSignature>(), reinterpret_cast<void**>(&dispatchIndirectSgn)));
  }

  allocator.setDevice(*this);
  descAlloc.setDevice(*this);
  dalloc.reset(new DxHeapAllocator(*this));

  static bool internalShaders = true;

  if(internalShaders) {
    {
    RenderState st;
    st.setZTestMode   (RenderState::ZTestMode::Always);
    st.setCullFaceMode(RenderState::CullMode::NoCull);

    auto blitVs = DSharedPtr<DxShader*>(new DxShader(blit_vert_sprv,sizeof(blit_vert_sprv)));
    auto blitFs = DSharedPtr<DxShader*>(new DxShader(blit_frag_sprv,sizeof(blit_frag_sprv)));

    DxShader* blitSh[2] = {blitVs.handler, blitFs.handler};
    blitLayout  = DSharedPtr<DxPipelineLay*>(new DxPipelineLay(*this,&blitFs.handler->lay));
    blit        = DSharedPtr<DxPipeline*>   (new DxPipeline   (*this,st,Triangles,*blitLayout.handler,blitSh,2));
    }

    {
    auto copyCs = DSharedPtr<DxShader*>(new DxShader(copy_comp_sprv,sizeof(copy_comp_sprv)));
    copyLayout  = DSharedPtr<DxPipelineLay*> (new DxPipelineLay(*this,&copyCs.handler->lay));
    copy        = DSharedPtr<DxCompPipeline*>(new DxCompPipeline(*this,*copyLayout.handler,*copyCs.handler));
    }
    {
    auto copyCs = DSharedPtr<DxShader*>(new DxShader(copy_s_comp_sprv,sizeof(copy_s_comp_sprv)));
    copyS       = DSharedPtr<DxCompPipeline*>(new DxCompPipeline(*this,*copyLayout.handler,*copyCs.handler));
    }
    }

  data.reset(new DataMgr(*this));
  }

DxDevice::~DxDevice() {
  blit       = DSharedPtr<DxPipeline*>();
  blitLayout = DSharedPtr<DxPipelineLay*>();
  data.reset();
  CloseHandle(idleEvent);
  }

void DxDevice::getProp(IDXGIAdapter1& adapter, ID3D12Device& dev, DxProps& prop) {
  DXGI_ADAPTER_DESC1 desc={};
  adapter.GetDesc1(&desc);
  return getProp(desc,dev,prop);
  }

void DxDevice::getProp(DXGI_ADAPTER_DESC1& desc, ID3D12Device& dev, DxProps& prop) {
  for(size_t i=0;i<sizeof(prop.name);++i)  {
    WCHAR c = desc.Description[i];
    if(c==0)
      break;
    if(('0'<=c && c<='9') || ('a'<=c && c<='z') || ('A'<=c && c<='Z') || c==' ' ||
       c=='(' || c==')' || c=='[' || c==']' || c=='{' || c=='}' ||
       c=='_' || c=='.' || c=='-' || c=='+')
      prop.name[i] = char(c); else
      prop.name[i] = '?';
    }
  prop.name[sizeof(prop.name)-1]='\0';

  // https://docs.microsoft.com/en-us/windows/win32/direct3ddxgi/hardware-support-for-direct3d-12-0-formats
  // NOTE: TextureFormat::RGB32F is not supported, because of mip-maps
  uint64_t smpBit = 0, attBit = 0, dsBit = 0, storBit = 0, atomBit = 0;

  for(uint32_t i=0; i<TextureFormat::Last; ++i){
    D3D12_FEATURE_DATA_FORMAT_SUPPORT d = {};
    d.Format = nativeFormat(TextureFormat(i));
    if(d.Format==DXGI_FORMAT_UNKNOWN || d.Format==DXGI_FORMAT_R32G32B32_FLOAT)
      continue;

    if(FAILED(dev.CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT, &d, sizeof(d))))
      continue;

    if(nativeIsDepthFormat(d.Format)) {
      D3D12_FEATURE_DATA_FORMAT_SUPPORT d2 = {};
      d2.Format = nativeSrvFormat(d.Format);
      if(SUCCEEDED(dev.CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT, &d2, sizeof(d2)))) {
        d.Support1 |= d2.Support1;
        d.Support2 |= d2.Support2;
        }
      }

    if(d.Support1 & D3D12_FORMAT_SUPPORT1_SHADER_SAMPLE &&
       d.Support1 & D3D12_FORMAT_SUPPORT1_SHADER_GATHER &&
       d.Support1 & D3D12_FORMAT_SUPPORT1_TEXTURE2D ) {
      smpBit |= uint64_t(1) << uint64_t(i);
      }
    if(d.Support1 & D3D12_FORMAT_SUPPORT1_SHADER_LOAD &&
       d.Support1 & D3D12_FORMAT_SUPPORT1_TEXTURE2D ) {
      // texelFetch only
      smpBit |= uint64_t(1) << uint64_t(i);
      }
    if(d.Support1 & D3D12_FORMAT_SUPPORT1_RENDER_TARGET &&
       d.Support1 & D3D12_FORMAT_SUPPORT1_TEXTURE2D&&
       !nativeIsDepthFormat(d.Format)) {
      attBit |= uint64_t(1) << uint64_t(i);
      }
    if(d.Support1 & D3D12_FORMAT_SUPPORT1_DEPTH_STENCIL &&
       d.Support1 & D3D12_FORMAT_SUPPORT1_TEXTURE2D) {
      dsBit |= uint64_t(1) << uint64_t(i);
      }
    if(d.Support2 & D3D12_FORMAT_SUPPORT2_UAV_TYPED_LOAD &&
       d.Support2 & D3D12_FORMAT_SUPPORT2_UAV_TYPED_STORE &&
       d.Support1 & D3D12_FORMAT_SUPPORT1_TEXTURE1D &&
       d.Support1 & D3D12_FORMAT_SUPPORT1_TEXTURE2D &&
       d.Support1 & D3D12_FORMAT_SUPPORT1_TEXTURE3D &&
       !nativeIsDepthFormat(d.Format)) {
      storBit |= uint64_t(1) << uint64_t(i);
      }
    if(d.Support2 & D3D12_FORMAT_SUPPORT2_UAV_ATOMIC_ADD &&
       d.Support2 & D3D12_FORMAT_SUPPORT2_UAV_ATOMIC_BITWISE_OPS &&
       d.Support2 & D3D12_FORMAT_SUPPORT2_UAV_ATOMIC_COMPARE_STORE_OR_COMPARE_EXCHANGE &&
       d.Support2 & D3D12_FORMAT_SUPPORT2_UAV_ATOMIC_EXCHANGE &&
       d.Support2 & D3D12_FORMAT_SUPPORT2_UAV_ATOMIC_UNSIGNED_MIN_OR_MAX &&
       d.Support1 & D3D12_FORMAT_SUPPORT1_TEXTURE1D &&
       d.Support1 & D3D12_FORMAT_SUPPORT1_TEXTURE2D &&
       d.Support1 & D3D12_FORMAT_SUPPORT1_TEXTURE3D &&
       !nativeIsDepthFormat(d.Format)) {
      atomBit |= uint64_t(1) << uint64_t(i);
      }
    }

  prop.setSamplerFormats(smpBit);
  prop.setAttachFormats (attBit);
  prop.setDepthFormats  (dsBit);
  prop.setStorageFormats(storBit);
  prop.setAtomicFormats (atomBit);

  // TODO: buffer limits
  //prop.vbo.maxStride    = ;

  prop.ssbo.offsetAlign = 256;
  //prop.ssbo.maxRange    = size_t(prop.limits.maxStorageBufferRange);

  prop.ubo.maxRange      = D3D12_REQ_CONSTANT_BUFFER_ELEMENT_COUNT*4;
  prop.ubo.offsetAlign   = 256;

  prop.push.maxRange     = 128;

  prop.anisotropy        = true;
  prop.maxAnisotropy     = 16;
  prop.tesselationShader = false; // TODO: dxil compiller crashes

  prop.storeAndAtomicVs  = true;
  prop.storeAndAtomicFs  = true;

  prop.render.maxColorAttachments  = D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT;
  prop.render.maxViewportSize.w    = D3D12_VIEWPORT_BOUNDS_MAX;
  prop.render.maxViewportSize.h    = D3D12_VIEWPORT_BOUNDS_MAX;
  prop.render.maxClipCullDistances = D3D12_CLIP_OR_CULL_DISTANCE_COUNT;

  prop.compute.maxGroups.x    = D3D12_CS_DISPATCH_MAX_THREAD_GROUPS_PER_DIMENSION;
  prop.compute.maxGroups.y    = D3D12_CS_DISPATCH_MAX_THREAD_GROUPS_PER_DIMENSION;
  prop.compute.maxGroups.z    = D3D12_CS_DISPATCH_MAX_THREAD_GROUPS_PER_DIMENSION;

  prop.compute.maxGroupSize.x  = D3D12_CS_THREAD_GROUP_MAX_X;
  prop.compute.maxGroupSize.y  = D3D12_CS_THREAD_GROUP_MAX_Y;
  prop.compute.maxGroupSize.z  = D3D12_CS_THREAD_GROUP_MAX_Z;
  prop.compute.maxInvocations  = D3D12_CS_THREAD_GROUP_MAX_THREADS_PER_GROUP;
  prop.compute.maxSharedMemory = D3D12_CS_TGSM_REGISTER_COUNT*4; // not well documented, assume that this is shared memory

  prop.tex2d.maxSize = D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION;
  prop.tex3d.maxSize = D3D12_REQ_TEXTURE3D_U_V_OR_W_DIMENSION;

  if(desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
    prop.type = DeviceType::Cpu;
    } else {
    D3D12_FEATURE_DATA_ARCHITECTURE arch = {};
    if(SUCCEEDED(dev.CheckFeatureSupport(D3D12_FEATURE_ARCHITECTURE, &arch, sizeof(arch))))
      prop.type = arch.UMA ? DeviceType::Integrated : DeviceType::Discrete;
    }

  D3D12_FEATURE_DATA_D3D12_OPTIONS feature0 = {};
  if(SUCCEEDED(dev.CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS, &feature0, sizeof(feature0)))) {
    prop.descriptors.nonUniformIndexing = true;  // SM5.1, mandatory in DX12
    switch(feature0.ResourceBindingTier) {
      case D3D12_RESOURCE_BINDING_TIER_1:
        prop.descriptors.maxSamplers = 16;
        prop.descriptors.maxTexture  = 64;
        prop.descriptors.maxStorage  = 8;  // 64 for feature levels 11.1, but pointless anyway
        break;
      case D3D12_RESOURCE_BINDING_TIER_2:
        prop.descriptors.maxSamplers = 2048;
        prop.descriptors.maxTexture  = 500'000;
        prop.descriptors.maxStorage  = 64;
        break;
      case D3D12_RESOURCE_BINDING_TIER_3:
        prop.descriptors.maxSamplers = 2048;
        prop.descriptors.maxTexture  = 500'000; // half-heap for both
        prop.descriptors.maxStorage  = 500'000;
        break;
      }
    }

  D3D12_FEATURE_DATA_D3D12_OPTIONS5 feature5 = {};
  if(SUCCEEDED(dev.CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &feature5, sizeof(feature5)))) {
    prop.raytracing.rayQuery = (feature5.RaytracingTier >= D3D12_RAYTRACING_TIER_1_1);
    }

  D3D12_FEATURE_DATA_D3D12_OPTIONS7 feature7 = {};
  if(SUCCEEDED(dev.CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS7, &feature7, sizeof(feature7)))) {
    prop.meshlets.taskShader = feature7.MeshShaderTier!=D3D12_MESH_SHADER_TIER_NOT_SUPPORTED;
    prop.meshlets.meshShader = feature7.MeshShaderTier!=D3D12_MESH_SHADER_TIER_NOT_SUPPORTED;
    // https://microsoft.github.io/DirectX-Specs/d3d/MeshShader.html#dispatchmesh-api
    // ThreadGroupCountX*ThreadGroupCountY*ThreadGroupCountZ must not exceed 2^22.
    prop.meshlets.maxGroups.x = D3D12_CS_DISPATCH_MAX_THREAD_GROUPS_PER_DIMENSION;
    prop.meshlets.maxGroups.y = D3D12_CS_DISPATCH_MAX_THREAD_GROUPS_PER_DIMENSION;
    prop.meshlets.maxGroups.z = D3D12_CS_DISPATCH_MAX_THREAD_GROUPS_PER_DIMENSION;
    // https://microsoft.github.io/DirectX-Specs/d3d/MeshShader.html#numthreads
    // The number of threads can not exceed X * Y * Z = 128
    prop.meshlets.maxGroupSize.x = 128;
    prop.meshlets.maxGroupSize.y = 128;
    prop.meshlets.maxGroupSize.z = 128;
    }

  D3D12_FEATURE_DATA_D3D12_OPTIONS12 options12 = {};
  if(SUCCEEDED(dev.CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS12, &options12, sizeof(options12)))) {
    prop.enhancedBarriers = options12.EnhancedBarriersSupported;
    }
  }

void DxDevice::waitIdle() {
  std::lock_guard<SpinLock> guard(syncCmdQueue);
  ++cmdProgress;
  assert(cmdProgress>0);
  dxAssert(cmdQueue->Signal(cmdFence.get(), cmdProgress), *this);
  dxAssert(cmdFence->SetEventOnCompletion(cmdProgress, idleEvent), *this);
  dxAssert(WaitForSingleObjectEx(idleEvent, INFINITE, FALSE), *this);
  }

std::shared_ptr<DxTimepoint> DxDevice::findAvailableFence() {
  const UINT64 v = cmdFence->GetCompletedValue();
  for(int pass=0; pass<2; ++pass) {
    for(uint32_t id=0; id<MaxFences; ++id) {
      auto& i = timeline.timepoint[id];
      if(i==nullptr)
        continue;
      if(i->signalValue>v)
        continue;
      if(i.use_count()>1 && pass==0)
        continue;
      if(i.use_count()>1) {
        auto event = std::move(i->event);
        //NOTE: application may still hold references to `i`
        i->event = DxEvent();
        i = std::make_shared<DxTimepoint>(this, std::move(event));
        }
      dxAssert(ResetEvent(i->event.hevt) ? S_OK : DXGI_ERROR_DEVICE_REMOVED);
      return i;
      }
    }
  return nullptr;
  }

void DxDevice::waitAny() {
  HANDLE   fences[MaxFences] = {};
  uint32_t num = 0;

  for(auto& i:timeline.timepoint) {
    if(i==nullptr)
      continue;
    fences[num] = i->event.hevt;
    ++num;
    }

  DWORD ret = WaitForMultipleObjectsEx(num, fences, FALSE, INFINITE, FALSE);
  if(ret==WAIT_TIMEOUT)
    return;
  if(WAIT_OBJECT_0<=ret && ret<WAIT_OBJECT_0+num)
    return;
  dxAssert(ret);
  }

std::shared_ptr<DxTimepoint> DxDevice::aquireFence() {
  std::lock_guard<std::mutex> guard(timeline.sync);

  // reuse signalled fences
  auto f = findAvailableFence();
  if(f!=nullptr)
    return f;

  // allocate one and add to the pool
  for(uint32_t id=0; id<MaxFences; ++id) {
    auto& i = timeline.timepoint[id];
    if(i!=nullptr)
      continue;

    i = std::make_shared<DxTimepoint>(this, DxEvent(false));
    return i;
    }

  //pool is full - wait on one of the fences and reuse it
  waitAny();
  return findAvailableFence();
  }

HRESULT DxDevice::waitFence(DxTimepoint& t, uint64_t timeout) {
  UINT64 v = cmdFence->GetCompletedValue();
  if(v>=t.signalValue)
    return S_OK;
  if(timeout==0)
    return WAIT_TIMEOUT;
  if(timeout>INFINITE)
    timeout = INFINITE;
  DWORD ret = WaitForSingleObjectEx(t.event.hevt, DWORD(timeout), FALSE);
  if(ret==WAIT_OBJECT_0)
    return S_OK;
  if(ret==WAIT_TIMEOUT)
    return ret;
  return ret;
  }

std::shared_ptr<DxTimepoint> DxDevice::submit(DxCommandBuffer& cmd) {
  const size_t                                 size = cmd.chunks.size();
  SmallArray<ID3D12CommandList*, MaxCmdChunks> flat(size);
  auto node = cmd.chunks.begin();
  for(size_t i=0; i<size; ++i) {
    flat[i] = node->val[i%cmd.chunks.chunkSize].impl;
    if(i+1==cmd.chunks.chunkSize)
      node = node->next;
    }

  auto pfence = aquireFence();
  if(pfence==nullptr)
    throw DeviceLostException();

  std::lock_guard<std::mutex> guard1(timeline.sync);
  std::lock_guard<SpinLock>   guard2(syncCmdQueue);

  ResetEvent(pfence->event.hevt);
  cmdQueue->ExecuteCommandLists(UINT(size), flat.get());
  ++cmdProgress;
  dxAssert(cmdQueue->Signal(cmdFence.get(), cmdProgress));
  dxAssert(cmdFence->SetEventOnCompletion(cmdProgress, pfence->event.hevt));

  pfence->signalValue = cmdProgress;
  return pfence;
  }

void DxDevice::debugReportCallback(
    D3D12_MESSAGE_CATEGORY category, D3D12_MESSAGE_SEVERITY severity, D3D12_MESSAGE_ID ID,
    LPCSTR pMessage, void* pContext) {
  (void)pContext;
  //Log::e(pMessage," object=",object,", type=",objectType," th:",std::this_thread::get_id());
  Log::e("DirectX12: ", pMessage," th:",std::this_thread::get_id());
  }

#endif
