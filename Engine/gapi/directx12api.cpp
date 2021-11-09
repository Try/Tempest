#include "directx12api.h"

#if defined(TEMPEST_BUILD_DIRECTX12)
#include <dxgi1_6.h>
#include <d3d12.h>

#include "directx12/guid.h"
#include "directx12/comptr.h"
#include "directx12/dxdevice.h"
#include "directx12/dxbuffer.h"
#include "directx12/dxtexture.h"
#include "directx12/dxshader.h"
#include "directx12/dxpipeline.h"
#include "directx12/dxpipelinelay.h"
#include "directx12/dxcommandbuffer.h"
#include "directx12/dxswapchain.h"
#include "directx12/dxfence.h"
#include "directx12/dxdescriptorarray.h"
#include "directx12/dxfbolayout.h"

#include <Tempest/Pixmap>

using namespace Tempest;
using namespace Tempest::Detail;

struct DirectX12Api::Impl {
  Impl(bool validation) {
    CoInitialize(nullptr);
    d3d12_dll = LoadLibraryA("d3d12.dll");
    initApi(dllApi);

    if(validation && dllApi.D3D12GetDebugInterface!=nullptr) {
      dxAssert(dllApi.D3D12GetDebugInterface(uuid<ID3D12Debug>(), reinterpret_cast<void**>(&D3D12DebugController)));
      D3D12DebugController->EnableDebugLayer();
      }
    // Note  Don't mix the use of DXGI 1.0 (IDXGIFactory) and DXGI 1.1 (IDXGIFactory1) in an application.
    // Use IDXGIFactory or IDXGIFactory1, but not both in an application.
    // https://docs.microsoft.com/en-us/windows/win32/api/d3d12/nf-d3d12-d3d12createdevice
    dxAssert(CreateDXGIFactory1(uuid<IDXGIFactory4>(), reinterpret_cast<void**>(&DXGIFactory)));
    }

  ~Impl(){
    }

  std::vector<Props> devices() {
    std::vector<Props> d;
    if(dllApi.D3D12CreateDevice==nullptr)
      return d;

    auto& dxgi = *DXGIFactory;
    ComPtr<IDXGIAdapter1> adapter;
    for(UINT i = 0; DXGI_ERROR_NOT_FOUND != dxgi.EnumAdapters1(i, &adapter.get()); ++i) {
      DXGI_ADAPTER_DESC1 desc={};
      adapter->GetDesc1(&desc);

      ComPtr<IDXGIAdapter3> adapter3;
      adapter->QueryInterface(__uuidof(IDXGIAdapter3), reinterpret_cast<void**>(&adapter3.get()));

      DXGI_ADAPTER_DESC2 desc2={};
      if(adapter3.get()!=nullptr)
        adapter3->GetDesc2(&desc2);

      AbstractGraphicsApi::Props props={};
      DxDevice::getProp(desc,props);
      auto hr = dllApi.D3D12CreateDevice(adapter.get(), D3D_FEATURE_LEVEL_11_0, uuid<ID3D12Device>(), nullptr);
      if(SUCCEEDED(hr))
        d.push_back(props);
      }

    return d;
    }

  AbstractGraphicsApi::Device* createDevice(const char* gpuName) {
    if(dllApi.D3D12CreateDevice==nullptr)
      throw std::system_error(Tempest::GraphicsErrc::NoDevice);

    auto& dxgi = *DXGIFactory;

    ComPtr<IDXGIAdapter1> adapter;
    for(UINT i = 0; DXGI_ERROR_NOT_FOUND != dxgi.EnumAdapters1(i, &adapter.get()); ++i) {
      DXGI_ADAPTER_DESC1 desc={};
      adapter->GetDesc1(&desc);
      if(desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
        continue;

      AbstractGraphicsApi::Props props={};
      DxDevice::getProp(desc,props);
      if(gpuName!=nullptr && std::strcmp(props.name,gpuName)!=0)
        continue;

      // Check to see if the adapter supports Direct3D 12, but don't create the
      // actual device yet.
      auto hr = dllApi.D3D12CreateDevice(adapter.get(), D3D_FEATURE_LEVEL_11_0, uuid<ID3D12Device>(), nullptr);
      if(SUCCEEDED(hr))
        break;
      }

    if(adapter.get()==nullptr)
      throw std::system_error(Tempest::GraphicsErrc::NoDevice);
    return new DxDevice(*adapter,dllApi);
    }

  void submit(AbstractGraphicsApi::Device* d,
              ID3D12CommandList** cmd, size_t count,
              AbstractGraphicsApi::Fence* doneCpu){
    Detail::DxDevice& dx   = *reinterpret_cast<Detail::DxDevice*>(d);
    Detail::DxFence&  fcpu = *reinterpret_cast<Detail::DxFence*>(doneCpu);

    std::lock_guard<SpinLock> guard(dx.syncCmdQueue);
    fcpu.reset();
    dx.cmdQueue->ExecuteCommandLists(UINT(count), cmd);
    fcpu.signal(*dx.cmdQueue);
    }

  void initApi(DxDevice::ApiEntry& a) {
    if(d3d12_dll==nullptr)
      return;
    getProcAddress(a.D3D12CreateDevice,          "D3D12CreateDevice");
    getProcAddress(a.D3D12GetDebugInterface,     "D3D12GetDebugInterface");
    getProcAddress(a.D3D12SerializeRootSignature,"D3D12SerializeRootSignature");
    }

  template<class T>
  void getProcAddress(T& t, const char* name) {
    t = reinterpret_cast<T>(GetProcAddress(d3d12_dll,name));
    }

  HMODULE                d3d12_dll;
  DxDevice::ApiEntry     dllApi;

  ComPtr<ID3D12Debug>    D3D12DebugController;
  ComPtr<IDXGIFactory4>  DXGIFactory;
  };

DirectX12Api::DirectX12Api(ApiFlags f) {
  impl.reset(new Impl(bool(f&ApiFlags::Validation)));
  }

DirectX12Api::~DirectX12Api(){
  }

std::vector<AbstractGraphicsApi::Props> DirectX12Api::devices() const {
  return impl->devices();
  }

AbstractGraphicsApi::Device* DirectX12Api::createDevice(const char* gpuName) {
  return impl->createDevice(gpuName);
  }

void DirectX12Api::destroy(AbstractGraphicsApi::Device* d) {
  delete d;
  }

AbstractGraphicsApi::Swapchain* DirectX12Api::createSwapchain(SystemApi::Window* w, AbstractGraphicsApi::Device* d) {
  auto* dx = reinterpret_cast<Detail::DxDevice*>(d);
  return new Detail::DxSwapchain(*dx,*impl->DXGIFactory,w);
  }

AbstractGraphicsApi::PPipeline DirectX12Api::createPipeline(AbstractGraphicsApi::Device* d, const RenderState& st, size_t stride,
                                                            Topology tp,
                                                            const PipelineLay& ulayImpl,
                                                            const Shader* vs, const Shader* tc, const Shader* te, const Shader* gs, const Shader* fs) {
  auto* dx   = reinterpret_cast<Detail::DxDevice*>(d);
  auto* vert = reinterpret_cast<const Detail::DxShader*>(vs);
  auto* ctrl = reinterpret_cast<const Detail::DxShader*>(tc);
  auto* tess = reinterpret_cast<const Detail::DxShader*>(te);
  auto* geom = reinterpret_cast<const Detail::DxShader*>(gs);
  auto* frag = reinterpret_cast<const Detail::DxShader*>(fs);
  auto& ul   = reinterpret_cast<const Detail::DxPipelineLay&>(ulayImpl);

  return PPipeline(new Detail::DxPipeline(*dx,st,stride,tp,ul,vert,ctrl,tess,geom,frag));
  }

AbstractGraphicsApi::PCompPipeline DirectX12Api::createComputePipeline(AbstractGraphicsApi::Device* d,
                                                                       const AbstractGraphicsApi::PipelineLay& ulayImpl,
                                                                       AbstractGraphicsApi::Shader* shader) {
  auto*   dx = reinterpret_cast<Detail::DxDevice*>(d);
  auto&   ul = reinterpret_cast<const Detail::DxPipelineLay&>(ulayImpl);

  return PCompPipeline(new Detail::DxCompPipeline(*dx,ul,*reinterpret_cast<Detail::DxShader*>(shader)));
  }

AbstractGraphicsApi::PShader DirectX12Api::createShader(AbstractGraphicsApi::Device*,
                                                        const void* source, size_t src_size) {
  return PShader(new Detail::DxShader(source,src_size));
  }

AbstractGraphicsApi::Fence* DirectX12Api::createFence(AbstractGraphicsApi::Device* d) {
  Detail::DxDevice* dx = reinterpret_cast<Detail::DxDevice*>(d);
  return new DxFence(*dx);
  }

AbstractGraphicsApi::PBuffer DirectX12Api::createBuffer(AbstractGraphicsApi::Device* d, const void* mem,
                                                        size_t count, size_t size, size_t alignedSz,
                                                        MemUsage usage, BufferHeap flg) {
  DxDevice& dx = *reinterpret_cast<DxDevice*>(d);

  const BufferHeap flgOrig = flg;
  if(MemUsage::StorageBuffer==(usage&MemUsage::StorageBuffer))
   flg = BufferHeap::Device;

  usage = usage|MemUsage::TransferSrc|MemUsage::TransferDst;

  if(flg==BufferHeap::Upload) {
    DxBuffer buf=dx.allocator.alloc(mem,count,size,alignedSz,usage,BufferHeap::Upload);
    return PBuffer(new DxBuffer(std::move(buf)));
    }

  if(flg!=flgOrig && flgOrig!=BufferHeap::Device) {
    DxBuffer                    base = dx.allocator.alloc(nullptr,count,size,alignedSz,usage,flg);
    Detail::DSharedPtr<Buffer*> buf(new DxBufferWithStaging(std::move(base),flgOrig));
    if(mem!=nullptr)
      buf.handler->update(mem,0,count,size,alignedSz);
    return buf;
    }

  DxBuffer base = dx.allocator.alloc(nullptr,count,size,alignedSz,usage,flg);
  Detail::DSharedPtr<Buffer*> buf(new Detail::DxBuffer(std::move(base)));
  if(mem!=nullptr)
    buf.handler->update(mem,0,count,size,alignedSz);
  return buf;
  }

AbstractGraphicsApi::Desc* DirectX12Api::createDescriptors(AbstractGraphicsApi::Device*, PipelineLay& layP) {
  Detail::DxPipelineLay& u = reinterpret_cast<Detail::DxPipelineLay&>(layP);
  return new DxDescriptorArray(u);
  }

AbstractGraphicsApi::PPipelineLay DirectX12Api::createPipelineLayout(Device * d,
                                                                     const Shader* vs, const Shader* tc, const Shader* te,
                                                                     const Shader* gs, const Shader* fs, const Shader* cs) {
  auto* dx = reinterpret_cast<Detail::DxDevice*>(d);
  if(cs!=nullptr) {
    auto* comp = reinterpret_cast<const Detail::DxShader*>(cs);
    return PPipelineLay(new Detail::DxPipelineLay(*dx,comp->lay));
    }

  const Shader* sh[] = {vs,tc,te,gs,fs};
  const std::vector<Detail::ShaderReflection::Binding>* lay[5] = {};
  for(size_t i=0; i<5; ++i) {
    if(sh[i]==nullptr)
      continue;
    auto* s = reinterpret_cast<const Detail::DxShader*>(sh[i]);
    lay[i] = &s->lay;
    }
  return PPipelineLay(new Detail::DxPipelineLay(*dx,lay,5));
  }

AbstractGraphicsApi::PTexture DirectX12Api::createTexture(Device* d, const Pixmap& p, TextureFormat frm, uint32_t mipCnt) {
  if(isCompressedFormat(frm))
    return createCompressedTexture(d,p,frm,mipCnt);

  Detail::DxDevice& dx     = *reinterpret_cast<Detail::DxDevice*>(d);
  DXGI_FORMAT       format = Detail::nativeFormat(frm);
  uint32_t          row    = p.w()*p.bpp();
  const uint32_t    pith   = ((row+D3D12_TEXTURE_DATA_PITCH_ALIGNMENT-1)/D3D12_TEXTURE_DATA_PITCH_ALIGNMENT)*D3D12_TEXTURE_DATA_PITCH_ALIGNMENT;
  Detail::DxBuffer  stage  = dx.allocator.alloc(p.data(),p.h(),row,pith,MemUsage::TransferSrc,BufferHeap::Upload);
  Detail::DxTexture buf    = dx.allocator.alloc(p,mipCnt,format);

  Detail::DSharedPtr<Buffer*>  pstage(new Detail::DxBuffer (std::move(stage)));
  Detail::DSharedPtr<Texture*> pbuf  (new Detail::DxTexture(std::move(buf)));

  auto cmd = dx.dataMgr().get();
  cmd->begin();
  cmd->hold(pbuf);
  cmd->hold(pstage); // preserve stage buffer, until gpu side copy is finished

  cmd->copy(*pbuf.handler,p.w(),p.h(),0,*pstage.handler,0);
  cmd->barrier(*pbuf.handler, ResourceAccess::TransferDst, ResourceAccess::Sampler, uint32_t(-1));
  if(mipCnt>1)
    cmd->generateMipmap(*pbuf.handler, p.w(), p.h(), mipCnt);
  cmd->end();
  dx.dataMgr().submit(std::move(cmd));
  return PTexture(pbuf.handler);
  }

AbstractGraphicsApi::PTexture DirectX12Api::createCompressedTexture(Device* d, const Pixmap& p, TextureFormat frm, uint32_t mipCnt) {
  Detail::DxDevice& dx     = *reinterpret_cast<Detail::DxDevice*>(d);

  DXGI_FORMAT    format    = Detail::nativeFormat(frm);
  Pixmap::Format pfrm      = Pixmap::toPixmapFormat(frm);
  UINT           blockSize = UINT(Pixmap::blockSizeForFormat(pfrm));

  size_t bufferSize      = 0;
  UINT   stageBufferSize = 0;
  uint32_t w = p.w(), h = p.h();

  for(uint32_t i=0; i<mipCnt; i++) {
    Size bsz   = Pixmap::blockCount(pfrm,w,h);
    UINT pitch = alignTo(bsz.w*blockSize,D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);

    bufferSize      += bsz.w*bsz.h*blockSize;
    stageBufferSize += pitch*bsz.h;
    stageBufferSize = alignTo(stageBufferSize,D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT);

    w = std::max<uint32_t>(1,w/2);
    h = std::max<uint32_t>(1,h/2);
    }

  Detail::DxBuffer  stage  = dx.allocator.alloc(nullptr,stageBufferSize,1,1,MemUsage::TransferSrc,BufferHeap::Upload);
  Detail::DxTexture buf    = dx.allocator.alloc(p,mipCnt,format);
  Detail::DSharedPtr<Buffer*>  pstage(new Detail::DxBuffer (std::move(stage)));
  Detail::DSharedPtr<Texture*> pbuf  (new Detail::DxTexture(std::move(buf)));

  reinterpret_cast<Detail::DxBuffer*>(pstage.handler)->uploadS3TC(reinterpret_cast<const uint8_t*>(p.data()),p.w(),p.h(),mipCnt,blockSize);

  auto cmd = dx.dataMgr().get();
  cmd->begin();
  cmd->hold(pbuf);
  cmd->hold(pstage); // preserve stage buffer, until gpu side copy is finished

  stageBufferSize = 0;
  w = p.w();
  h = p.h();
  for(uint32_t i=0; i<mipCnt; i++) {
    UINT wBlk = (w+3)/4;
    UINT hBlk = (h+3)/4;

    cmd->copy(*pbuf.handler,w,h,i,*pstage.handler,stageBufferSize);

    UINT pitch = wBlk*blockSize;
    pitch = alignTo(pitch,D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);

    stageBufferSize += pitch*hBlk;
    stageBufferSize = alignTo(stageBufferSize,D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT);

    w = std::max<uint32_t>(4,w/2);
    h = std::max<uint32_t>(4,h/2);
    }

  cmd->barrier(*pbuf.handler, ResourceAccess::TransferDst, ResourceAccess::Sampler, uint32_t(-1));
  cmd->end();
  dx.dataMgr().submit(std::move(cmd));
  return PTexture(pbuf.handler);
  }

AbstractGraphicsApi::PTexture DirectX12Api::createTexture(Device* d, const uint32_t w, const uint32_t h, uint32_t mipCnt, TextureFormat frm) {
  DxDevice& dx = *reinterpret_cast<DxDevice*>(d);

  DxTexture base=dx.allocator.alloc(w,h,mipCnt,frm,false);
  DSharedPtr<DxTexture*> pbuf(new DxTextureWithRT(dx,std::move(base)));

  return PTexture(pbuf.handler);
  }

AbstractGraphicsApi::PTexture DirectX12Api::createStorage(AbstractGraphicsApi::Device* d, const uint32_t w, const uint32_t h, uint32_t mipCnt, TextureFormat frm) {
  Detail::DxDevice& dx = *reinterpret_cast<Detail::DxDevice*>(d);

  Detail::DxTexture buf=dx.allocator.alloc(w,h,mipCnt,frm,true);
  Detail::DSharedPtr<Detail::DxTexture*> pbuf(new Detail::DxTexture(std::move(buf)));

  return PTexture(pbuf.handler);
  }

void DirectX12Api::readPixels(Device* d, Pixmap& out, const PTexture t,
                              TextureFormat frm, const uint32_t w, const uint32_t h, uint32_t mip, bool storageImg) {
  Detail::DxDevice&  dx = *reinterpret_cast<Detail::DxDevice*>(d);
  Detail::DxTexture& tx = *reinterpret_cast<Detail::DxTexture*>(t.handler);

  Pixmap::Format   pfrm   = Pixmap::toPixmapFormat(frm);
  size_t           bpb    = Pixmap::blockSizeForFormat(pfrm);
  Size             bsz    = Pixmap::blockCount(pfrm,w,h);

  uint32_t         row    = bsz.w*uint32_t(bpb);
  const uint32_t   pith   = ((row+D3D12_TEXTURE_DATA_PITCH_ALIGNMENT-1)/D3D12_TEXTURE_DATA_PITCH_ALIGNMENT)*D3D12_TEXTURE_DATA_PITCH_ALIGNMENT;
  Detail::DxBuffer stage  = dx.allocator.alloc(nullptr,bsz.h,bsz.w*bpb,pith,MemUsage::TransferDst,BufferHeap::Readback);
  ResourceAccess   defLay = storageImg ? ResourceAccess::ComputeRead : ResourceAccess::Sampler;

  auto cmd = dx.dataMgr().get();
  cmd->begin();
  cmd->barrier(tx,defLay,ResourceAccess::TransferSrc,mip);
  cmd->copyNative(stage,0, tx,w,h,mip);
  cmd->barrier(tx,ResourceAccess::TransferSrc,defLay,mip);
  cmd->end();
  dx.dataMgr().submitAndWait(std::move(cmd));

  out = Pixmap(w,h,pfrm);
  for(int32_t i=0; i<bsz.h; ++i)
    stage.read(reinterpret_cast<uint8_t*>(out.data())+i*bsz.w*bpb, i*pith, bsz.w*bpb);
  }

void DirectX12Api::readBytes(AbstractGraphicsApi::Device*, AbstractGraphicsApi::Buffer* buf, void* out, size_t size) {
  buf->read(out,0,size);
  }

AbstractGraphicsApi::CommandBuffer* DirectX12Api::createCommandBuffer(Device* d) {
  Detail::DxDevice* dx = reinterpret_cast<Detail::DxDevice*>(d);
  return new DxCommandBuffer(*dx);
  }

void DirectX12Api::present(AbstractGraphicsApi::Device* d, AbstractGraphicsApi::Swapchain* sw) {
  Detail::DxDevice&    dx = *reinterpret_cast<Detail::DxDevice*>(d);
  Detail::DxSwapchain* sx = reinterpret_cast<Detail::DxSwapchain*>(sw);

  std::lock_guard<SpinLock> guard(dx.syncCmdQueue);
  sx->queuePresent();
  }

void DirectX12Api::submit(AbstractGraphicsApi::Device* d, AbstractGraphicsApi::CommandBuffer* cmd, AbstractGraphicsApi::Fence* doneCpu) {
  Detail::DxDevice&        dx = *reinterpret_cast<Detail::DxDevice*>(d);
  Detail::DxCommandBuffer& bx = *reinterpret_cast<Detail::DxCommandBuffer*>(cmd);
  ID3D12CommandList* cmdList[] = { bx.get() };

  dx.waitData();
  impl->submit(d,cmdList,1,doneCpu);
  }

void DirectX12Api::submit(AbstractGraphicsApi::Device* d, AbstractGraphicsApi::CommandBuffer** cmd, size_t count, AbstractGraphicsApi::Fence* doneCpu) {
  Detail::DxDevice&                     dx = *reinterpret_cast<Detail::DxDevice*>(d);
  ID3D12CommandList*                    cmdListStk[16]={};
  std::unique_ptr<ID3D12CommandList*[]> cmdListHeap;
  ID3D12CommandList**                   cmdList = cmdListStk;
  if(count>16) {
    cmdListHeap.reset(new ID3D12CommandList*[16]);
    cmdList = cmdListHeap.get();
    }
  for(size_t i=0;i<count;++i) {
    Detail::DxCommandBuffer& bx = *reinterpret_cast<Detail::DxCommandBuffer*>(cmd[i]);
    cmdList[i] = bx.get();
    }
  dx.waitData();
  impl->submit(d,cmdList,count,doneCpu);
  }

void DirectX12Api::getCaps(AbstractGraphicsApi::Device* d, AbstractGraphicsApi::Props& caps) {
  Detail::DxDevice& dx = *reinterpret_cast<Detail::DxDevice*>(d);
  caps = dx.props;
  }


#endif
