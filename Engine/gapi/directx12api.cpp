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
#include "directx12/dxuniformslay.h"
#include "directx12/dxcommandbuffer.h"
#include "directx12/dxswapchain.h"
#include "directx12/dxfence.h"
#include "directx12/dxframebuffer.h"
#include "directx12/dxdescriptorarray.h"
#include "directx12/dxrenderpass.h"
#include "directx12/dxfbolayout.h"

#include <Tempest/Pixmap>

using namespace Tempest;
using namespace Tempest::Detail;

struct DirectX12Api::Impl {
  Impl(bool validation) {
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
              AbstractGraphicsApi::Semaphore** wait, size_t waitCnt,
              AbstractGraphicsApi::Semaphore** done, size_t doneCnt,
              AbstractGraphicsApi::Fence* doneCpu){
    Detail::DxDevice& dx   = *reinterpret_cast<Detail::DxDevice*>(d);
    Detail::DxFence&  fcpu = *reinterpret_cast<Detail::DxFence*>(doneCpu);

    std::lock_guard<SpinLock> guard(dx.syncCmdQueue);
    fcpu.reset();

    for(size_t i=0;i<waitCnt;++i) {
      Detail::DxSemaphore& swait = *reinterpret_cast<Detail::DxSemaphore*>(wait[i]);
      dx.cmdQueue->Wait(swait.impl.get(),DxFence::Ready);
      }

    dx.cmdQueue->ExecuteCommandLists(UINT(count), cmd);

    for(size_t i=0;i<doneCnt;++i) {
      Detail::DxSemaphore& sgpu = *reinterpret_cast<Detail::DxSemaphore*>(done[i]);
      dx.cmdQueue->Signal(sgpu.impl.get(),DxFence::Ready);
      }
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

AbstractGraphicsApi::PPass DirectX12Api::createPass(AbstractGraphicsApi::Device*, const FboMode** att, size_t acount) {
  return PPass(new DxRenderPass(att,acount));
  }

AbstractGraphicsApi::PFbo DirectX12Api::createFbo(Device* d, FboLayout* l,
                                                  uint32_t /*w*/, uint32_t /*h*/, uint8_t clCount,
                                                  Swapchain** sx, Texture** cl, const uint32_t* imgId, Texture* zbuf) {
  auto& dx  = *reinterpret_cast<Detail::DxDevice*>   (d);
  auto& lay = *reinterpret_cast<Detail::DxFboLayout*>(l);
  auto  z   =  reinterpret_cast<Detail::DxTexture*>  (zbuf);

  Detail::DxTexture*   att[256] = {};
  Detail::DxSwapchain* sw [256] = {};
  for(size_t i=0; i<clCount; ++i) {
    att[i] = reinterpret_cast<Detail::DxTexture*>(cl[i]);
    sw[i]  = reinterpret_cast<Detail::DxSwapchain*>(sx[i]);
    }

  return PFbo(new DxFramebuffer(dx,lay,clCount, sw,att,imgId,z));
  }

AbstractGraphicsApi::PFboLayout DirectX12Api::createFboLayout(AbstractGraphicsApi::Device*, AbstractGraphicsApi::Swapchain** s,
                                                              TextureFormat* att, uint8_t attCount) {
  Detail::DxSwapchain* sx[256] = {};
  for(size_t i=0; i<attCount; ++i)
    sx[i] = reinterpret_cast<Detail::DxSwapchain*>(s[i]);
  return PFboLayout(new DxFboLayout(sx,att,attCount));
  }

AbstractGraphicsApi::PPipeline DirectX12Api::createPipeline(AbstractGraphicsApi::Device* d, const RenderState& st, size_t stride,
                                                            Topology tp,
                                                            const UniformsLay& ulayImpl,
                                                            const Shader* vs, const Shader* tc, const Shader* te, const Shader* gs, const Shader* fs) {
  auto* dx   = reinterpret_cast<Detail::DxDevice*>(d);
  auto* vert = reinterpret_cast<const Detail::DxShader*>(vs);
  auto* ctrl = reinterpret_cast<const Detail::DxShader*>(tc);
  auto* tess = reinterpret_cast<const Detail::DxShader*>(te);
  auto* geom = reinterpret_cast<const Detail::DxShader*>(gs);
  auto* frag = reinterpret_cast<const Detail::DxShader*>(fs);
  auto& ul   = reinterpret_cast<const Detail::DxUniformsLay&>(ulayImpl);

  return PPipeline(new Detail::DxPipeline(*dx,st,stride,tp,ul,vert,ctrl,tess,geom,frag));
  }

AbstractGraphicsApi::PCompPipeline DirectX12Api::createComputePipeline(AbstractGraphicsApi::Device* d,
                                                                       const AbstractGraphicsApi::UniformsLay& ulayImpl,
                                                                       AbstractGraphicsApi::Shader* shader) {
  auto*   dx = reinterpret_cast<Detail::DxDevice*>(d);
  auto&   ul = reinterpret_cast<const Detail::DxUniformsLay&>(ulayImpl);

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

AbstractGraphicsApi::Semaphore* DirectX12Api::createSemaphore(AbstractGraphicsApi::Device* d) {
  Detail::DxDevice* dx = reinterpret_cast<Detail::DxDevice*>(d);
  return new DxSemaphore(*dx);
  }

AbstractGraphicsApi::PBuffer DirectX12Api::createBuffer(AbstractGraphicsApi::Device* d, const void* mem,
                                                        size_t count, size_t size, size_t alignedSz,
                                                        MemUsage usage, BufferHeap flg) {
  Detail::DxDevice& dx = *reinterpret_cast<Detail::DxDevice*>(d);

  if(MemUsage::StorageBuffer==(usage&MemUsage::StorageBuffer))
    flg = BufferHeap::Device;

  if(flg==BufferHeap::Upload) {
    Detail::DxBuffer stage=dx.allocator.alloc(mem,count,size,alignedSz,usage,BufferHeap::Upload);
    return PBuffer(new Detail::DxBuffer(std::move(stage)));
    }

  Detail::DxBuffer  buf = dx.allocator.alloc(nullptr,count,size,alignedSz,usage|MemUsage::TransferDst,BufferHeap::Device);
  if(mem==nullptr) {
    Detail::DSharedPtr<Buffer*> pbuf(new Detail::DxBuffer(std::move(buf)));
    return PBuffer(pbuf.handler);
    }
  Detail::DSharedPtr<Buffer*> pbuf  (new Detail::DxBuffer(std::move(buf)));
  pbuf.handler->update(mem,0,count,size,alignedSz);
  return PBuffer(pbuf.handler);
  }

AbstractGraphicsApi::Desc* DirectX12Api::createDescriptors(AbstractGraphicsApi::Device*, UniformsLay& layP) {
  Detail::DxUniformsLay& u = reinterpret_cast<Detail::DxUniformsLay&>(layP);
  return new DxDescriptorArray(u);
  }

AbstractGraphicsApi::PUniformsLay DirectX12Api::createUboLayout(Device* d,
                                                                const Shader* vs, const Shader* tc, const Shader* te,
                                                                const Shader* gs, const Shader* fs, const Shader* cs) {
  auto* dx = reinterpret_cast<Detail::DxDevice*>(d);
  if(cs!=nullptr) {
    auto* comp = reinterpret_cast<const Detail::DxShader*>(cs);
    return PUniformsLay(new Detail::DxUniformsLay(*dx,comp->lay));
    }

  const Shader* sh[] = {vs,tc,te,gs,fs};
  const std::vector<Detail::ShaderReflection::Binding>* lay[5] = {};
  for(size_t i=0; i<5; ++i) {
    if(sh[i]==nullptr)
      continue;
    auto* s = reinterpret_cast<const Detail::DxShader*>(sh[i]);
    lay[i] = &s->lay;
    }
  return PUniformsLay(new Detail::DxUniformsLay(*dx,lay,5));
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
  if(mipCnt>1)
    cmd->generateMipmap(*pbuf.handler, TextureLayout::TransferDest, p.w(), p.h(), mipCnt); else
    cmd->changeLayout(*pbuf.handler, TextureLayout::TransferDest, TextureLayout::Sampler, uint32_t(-1));
  cmd->end();
  dx.dataMgr().submit(std::move(cmd));
  return PTexture(pbuf.handler);
  }

AbstractGraphicsApi::PTexture DirectX12Api::createCompressedTexture(Device* d, const Pixmap& p, TextureFormat frm, uint32_t mipCnt) {
  Detail::DxDevice& dx     = *reinterpret_cast<Detail::DxDevice*>(d);

  DXGI_FORMAT format    = Detail::nativeFormat(frm);
  UINT        blockSize = (frm==TextureFormat::DXT1) ? 8 : 16;

  size_t bufferSize      = 0;
  UINT   stageBufferSize = 0;
  uint32_t w = p.w(), h = p.h();

  for(uint32_t i=0; i<mipCnt; i++){
    UINT wBlk = (w+3)/4;
    UINT hBlk = (h+3)/4;
    UINT pitch = wBlk*blockSize;
    pitch = alignTo(pitch,D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);
    bufferSize += wBlk*hBlk*blockSize;

    stageBufferSize += pitch*hBlk;
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

  cmd->changeLayout(*pbuf.handler, TextureLayout::TransferDest, TextureLayout::Sampler, uint32_t(-1));
  cmd->end();
  dx.dataMgr().submit(std::move(cmd));
  return PTexture(pbuf.handler);
  }

AbstractGraphicsApi::PTexture DirectX12Api::createTexture(Device* d, const uint32_t w, const uint32_t h, uint32_t mipCnt, TextureFormat frm) {
  Detail::DxDevice& dx = *reinterpret_cast<Detail::DxDevice*>(d);

  Detail::DxTexture buf=dx.allocator.alloc(w,h,mipCnt,frm,false);
  Detail::DSharedPtr<Detail::DxTexture*> pbuf(new Detail::DxTexture(std::move(buf)));

  return PTexture(pbuf.handler);
  }

AbstractGraphicsApi::PTexture DirectX12Api::createStorage(AbstractGraphicsApi::Device* d, const uint32_t w, const uint32_t h, uint32_t mipCnt, TextureFormat frm) {
  Detail::DxDevice& dx = *reinterpret_cast<Detail::DxDevice*>(d);

  Detail::DxTexture buf=dx.allocator.alloc(w,h,mipCnt,frm,true);
  Detail::DSharedPtr<Detail::DxTexture*> pbuf(new Detail::DxTexture(std::move(buf)));

  return PTexture(pbuf.handler);
  }

void DirectX12Api::readPixels(Device* d, Pixmap& out, const PTexture t, TextureLayout lay,
                              TextureFormat frm, const uint32_t w, const uint32_t h, uint32_t mip) {
  Detail::DxDevice&  dx = *reinterpret_cast<Detail::DxDevice*>(d);
  Detail::DxTexture& tx = *reinterpret_cast<Detail::DxTexture*>(t.handler);

  Pixmap::Format  pfrm  = Pixmap::toPixmapFormat(frm);
  size_t          bpp   = Pixmap::bppForFormat(pfrm);
  if(bpp==0)
    throw std::runtime_error("not implemented");

  uint32_t         row   = w*uint32_t(bpp);
  const uint32_t   pith  = ((row+D3D12_TEXTURE_DATA_PITCH_ALIGNMENT-1)/D3D12_TEXTURE_DATA_PITCH_ALIGNMENT)*D3D12_TEXTURE_DATA_PITCH_ALIGNMENT;
  Detail::DxBuffer stage = dx.allocator.alloc(nullptr,h,w*bpp,pith,MemUsage::TransferDst,BufferHeap::Readback);

  auto cmd = dx.dataMgr().get();
  cmd->begin();
  cmd->changeLayout(tx, lay, TextureLayout::TransferSrc, mip);
  cmd->copy(stage,w,h,mip,tx,0);
  cmd->changeLayout(tx, TextureLayout::TransferSrc, lay, mip);
  cmd->end();
  dx.dataMgr().submitAndWait(std::move(cmd));

  out = Pixmap(w,h,pfrm);
  for(size_t i=0; i<h; ++i) {
    stage.read(reinterpret_cast<uint8_t*>(out.data())+i*w*bpp, i*pith, w*bpp);
    }
  }

void DirectX12Api::readBytes(AbstractGraphicsApi::Device*, AbstractGraphicsApi::Buffer* buf, void* out, size_t size) {
  buf->read(out,0,size);
  }

AbstractGraphicsApi::CommandBuffer* DirectX12Api::createCommandBuffer(Device* d) {
  Detail::DxDevice* dx = reinterpret_cast<Detail::DxDevice*>(d);
  return new DxCommandBuffer(*dx);
  }

void DirectX12Api::present(AbstractGraphicsApi::Device* d, AbstractGraphicsApi::Swapchain* sw,
                           uint32_t imageId, const AbstractGraphicsApi::Semaphore* wait) {
  // TODO: handle imageId
  (void)imageId;
  Detail::DxDevice&    dx    = *reinterpret_cast<Detail::DxDevice*>(d);
  auto&                swait = *reinterpret_cast<const Detail::DxSemaphore*>(wait);
  Detail::DxSwapchain* sx    = reinterpret_cast<Detail::DxSwapchain*>(sw);

  std::lock_guard<SpinLock> guard(dx.syncCmdQueue);
  dx.cmdQueue->Wait(swait.impl.get(),DxFence::Ready);
  sx->queuePresent();
  }

void DirectX12Api::submit(AbstractGraphicsApi::Device* d, AbstractGraphicsApi::CommandBuffer* cmd,
                          AbstractGraphicsApi::Semaphore* wait,
                          AbstractGraphicsApi::Semaphore* done, AbstractGraphicsApi::Fence* doneCpu) {
  Detail::DxDevice&        dx = *reinterpret_cast<Detail::DxDevice*>(d);
  Detail::DxCommandBuffer& bx = *reinterpret_cast<Detail::DxCommandBuffer*>(cmd);
  ID3D12CommandList* cmdList[] = { bx.impl.get() };

  dx.waitData();
  impl->submit(d,cmdList,1,&wait,1,&done,1,doneCpu);
  }

void DirectX12Api::submit(AbstractGraphicsApi::Device* d, AbstractGraphicsApi::CommandBuffer** cmd, size_t count,
                          AbstractGraphicsApi::Semaphore** wait, size_t waitCnt, AbstractGraphicsApi::Semaphore** done,
                          size_t doneCnt, AbstractGraphicsApi::Fence* doneCpu) {
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
    cmdList[i] = bx.impl.get();
    }
  dx.waitData();
  impl->submit(d,cmdList,count,wait,waitCnt,done,doneCnt,doneCpu);
  }

void DirectX12Api::getCaps(AbstractGraphicsApi::Device* d, AbstractGraphicsApi::Props& caps) {
  Detail::DxDevice& dx = *reinterpret_cast<Detail::DxDevice*>(d);
  caps = dx.props;
  }


#endif
