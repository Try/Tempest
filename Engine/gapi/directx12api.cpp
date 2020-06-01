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

#include <Tempest/Pixmap>

using namespace Tempest;
using namespace Tempest::Detail;

struct DirectX12Api::Impl {
  Impl(bool validation) {
    if(validation) {
      dxAssert(D3D12GetDebugInterface(uuid<ID3D12Debug>(), reinterpret_cast<void**>(&D3D12DebugController)));
      D3D12DebugController->EnableDebugLayer();
      }
    dxAssert(CreateDXGIFactory1(uuid<IDXGIFactory4>(), reinterpret_cast<void**>(&DXGIFactory)));
    }

  ComPtr<ID3D12Debug>    D3D12DebugController;
  ComPtr<IDXGIFactory4>  DXGIFactory;

  void submit(AbstractGraphicsApi::Device* d,
              ID3D12CommandList** cmd, size_t count,
              AbstractGraphicsApi::Semaphore** wait, size_t waitCnt,
              AbstractGraphicsApi::Semaphore** done, size_t doneCnt,
              AbstractGraphicsApi::Fence* doneCpu){
    Detail::DxDevice& dx   = *reinterpret_cast<Detail::DxDevice*>(d);
    Detail::DxFence&  fcpu = *reinterpret_cast<Detail::DxFence*>(doneCpu);

    fcpu.reset();
    //dx.cmdQueue->Signal(fcpu.impl.get(),0);

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
  };

DirectX12Api::DirectX12Api(ApiFlags f) {
  impl.reset(new Impl(bool(f&ApiFlags::Validation)));
  }

DirectX12Api::~DirectX12Api(){
  }

AbstractGraphicsApi::Device* DirectX12Api::createDevice(const char* gpuName) {
  return new DxDevice(*impl->DXGIFactory);
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

AbstractGraphicsApi::PFbo DirectX12Api::createFbo(AbstractGraphicsApi::Device* d, AbstractGraphicsApi::FboLayout*,
                                                  AbstractGraphicsApi::Swapchain* s, uint32_t imageId) {
  auto& dx = *reinterpret_cast<Detail::DxDevice*>   (d);
  auto& sx = *reinterpret_cast<Detail::DxSwapchain*>(s);
  return PFbo(new DxFramebuffer(dx,sx,imageId));
  }

AbstractGraphicsApi::PFbo DirectX12Api::createFbo(AbstractGraphicsApi::Device* d, AbstractGraphicsApi::FboLayout*, AbstractGraphicsApi::Swapchain* s,
                                                  uint32_t imageId, AbstractGraphicsApi::Texture* zbuf) {
  auto& dx = *reinterpret_cast<Detail::DxDevice*>   (d);
  auto& sx = *reinterpret_cast<Detail::DxSwapchain*>(s);
  auto& z  = *reinterpret_cast<Detail::DxTexture*>(zbuf);
  return PFbo(new DxFramebuffer(dx,sx,imageId,z));
  }

AbstractGraphicsApi::PFbo DirectX12Api::createFbo(AbstractGraphicsApi::Device* d, AbstractGraphicsApi::FboLayout*,
                                                  uint32_t /*w*/, uint32_t /*h*/,
                                                  AbstractGraphicsApi::Texture* cl, AbstractGraphicsApi::Texture* zbuf) {
  auto& dx = *reinterpret_cast<Detail::DxDevice*> (d);
  auto& t0 = *reinterpret_cast<Detail::DxTexture*>(cl);
  auto& z  = *reinterpret_cast<Detail::DxTexture*>(zbuf);
  return PFbo(new DxFramebuffer(dx,t0,z));
  }

AbstractGraphicsApi::PFbo DirectX12Api::createFbo(AbstractGraphicsApi::Device* d, AbstractGraphicsApi::FboLayout*,
                                                  uint32_t /*w*/, uint32_t /*h*/, AbstractGraphicsApi::Texture* cl) {
  auto& dx = *reinterpret_cast<Detail::DxDevice*> (d);
  auto& t0 = *reinterpret_cast<Detail::DxTexture*>(cl);
  return PFbo(new DxFramebuffer(dx,t0));
  }

AbstractGraphicsApi::PFboLayout DirectX12Api::createFboLayout(AbstractGraphicsApi::Device* d, AbstractGraphicsApi::Swapchain* s,
                                                              TextureFormat* att, size_t attCount) {
  return PFboLayout();
  }

AbstractGraphicsApi::PPipeline DirectX12Api::createPipeline(AbstractGraphicsApi::Device* d, const RenderState& st,
                                                            const Decl::ComponentType* decl, size_t declSize, size_t stride,
                                                            Topology tp,
                                                            const UniformsLay& ulayImpl,
                                                            const std::initializer_list<AbstractGraphicsApi::Shader*>& shaders) {
  Shader*const*        arr=shaders.begin();
  auto* dx = reinterpret_cast<Detail::DxDevice*>(d);
  auto* vs = reinterpret_cast<Detail::DxShader*>(arr[0]);
  auto* fs = reinterpret_cast<Detail::DxShader*>(arr[1]);
  auto& ul = reinterpret_cast<const Detail::DxUniformsLay&>(ulayImpl);

  return PPipeline(new Detail::DxPipeline(*dx,st,decl,declSize,stride,tp,ul,*vs,*fs));
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
                                                        MemUsage usage, BufferFlags flg) {
  Detail::DxDevice& dx = *reinterpret_cast<Detail::DxDevice*>(d);

  if(flg==BufferFlags::Dynamic) {
    Detail::DxBuffer stage=dx.allocator.alloc(mem,count,size,alignedSz,usage,BufferFlags::Dynamic);
    return PBuffer(new Detail::DxBuffer(std::move(stage)));
    }
  if(flg==BufferFlags::Staging) {
    Detail::DxBuffer stage=dx.allocator.alloc(mem,count,size,alignedSz,usage,BufferFlags::Staging);
    return PBuffer(new Detail::DxBuffer(std::move(stage)));
    }
  else {
    Detail::DxBuffer  stage=dx.allocator.alloc(mem,    count,size,alignedSz,MemUsage::TransferSrc,      BufferFlags::Staging);
    Detail::DxBuffer  buf  =dx.allocator.alloc(nullptr,count,size,alignedSz,usage|MemUsage::TransferDst,BufferFlags::Static );

    Detail::DSharedPtr<Detail::DxBuffer*> pstage(new Detail::DxBuffer(std::move(stage)));
    Detail::DSharedPtr<Detail::DxBuffer*> pbuf  (new Detail::DxBuffer(std::move(buf)));

    DxDevice::Data dat(dx);
    dat.flush(*pstage.handler,count*alignedSz);
    dat.hold(pbuf);
    dat.hold(pstage); // preserve stage buffer, until gpu side copy is finished
    dat.copy(*pbuf.handler,*pstage.handler,count*alignedSz);
    dat.commit();

    return PBuffer(pbuf.handler);
    }
  return PBuffer();
  }

AbstractGraphicsApi::Desc* DirectX12Api::createDescriptors(AbstractGraphicsApi::Device* d, const UniformsLayout& lay, UniformsLay& layP) {
  Detail::DxDevice&      dx = *reinterpret_cast<Detail::DxDevice*>(d);
  Detail::DxUniformsLay& u  = reinterpret_cast<Detail::DxUniformsLay&>(layP);
  return new DxDescriptorArray(dx,lay,u);
  }

AbstractGraphicsApi::PUniformsLay DirectX12Api::createUboLayout(Device* d, const UniformsLayout& lay) {
  Detail::DxDevice& dx = *reinterpret_cast<Detail::DxDevice*>(d);
  return PUniformsLay(new DxUniformsLay(dx,lay));
  }

AbstractGraphicsApi::PTexture DirectX12Api::createTexture(Device* d, const Pixmap& p, TextureFormat frm, uint32_t mipCnt) {
  Detail::DxDevice& dx     = *reinterpret_cast<Detail::DxDevice*>(d);
  //const uint32_t    size   = uint32_t(p.dataSize());
  DXGI_FORMAT       format = Detail::nativeFormat(frm);
  const uint32_t    row    = p.w()*p.bpp();
  const uint32_t    pith   = ((row+D3D12_TEXTURE_DATA_PITCH_ALIGNMENT-1)/D3D12_TEXTURE_DATA_PITCH_ALIGNMENT)*D3D12_TEXTURE_DATA_PITCH_ALIGNMENT;
  Detail::DxBuffer  stage  = dx.allocator.alloc(p.data(),p.h(),row,pith,MemUsage::TransferSrc,BufferFlags::Staging);
  Detail::DxTexture buf    = dx.allocator.alloc(p,mipCnt,format);

  Detail::DSharedPtr<Detail::DxBuffer*>  pstage(new Detail::DxBuffer (std::move(stage)));
  Detail::DSharedPtr<Detail::DxTexture*> pbuf  (new Detail::DxTexture(std::move(buf)));

  Detail::DxDevice::Data dat(dx);
  dat.hold(pstage);
  dat.hold(pbuf);

  // dat.changeLayout(*pbuf.handler, frm, TextureLayout::Undefined, TextureLayout::TransferDest, mipCnt);
  if(isCompressedFormat(frm)){
    size_t blocksize  = (frm==TextureFormat::DXT1) ? 8 : 16;
    size_t bufferSize = 0;

    uint32_t w = uint32_t(p.w()), h = uint32_t(p.h());
    for(uint32_t i=0; i<mipCnt; i++){
      size_t blockcount = ((w+3)/4)*((h+3)/4);
      dat.copy(*pbuf.handler,w,h,i,*pstage.handler,bufferSize);

      bufferSize += blockcount*blocksize;
      w = std::max<uint32_t>(1,w/2);
      h = std::max<uint32_t>(1,h/2);
      }

    dat.changeLayout(*pbuf.handler, frm, TextureLayout::TransferDest, TextureLayout::Sampler, mipCnt);
    } else {
    dat.copy(*pbuf.handler,p.w(),p.h(),0,*pstage.handler,0);
    if(mipCnt>1)
      dat.generateMipmap(*pbuf.handler,frm,p.w(),p.h(),mipCnt); else
      dat.changeLayout(*pbuf.handler, frm, TextureLayout::TransferDest, TextureLayout::Sampler,mipCnt);
    }
  dat.commit();
  return PTexture(pbuf.handler);
  }

AbstractGraphicsApi::PTexture DirectX12Api::createTexture(Device* d, const uint32_t w, const uint32_t h, uint32_t mipCnt, TextureFormat frm) {
  Detail::DxDevice& dx = *reinterpret_cast<Detail::DxDevice*>(d);

  Detail::DxTexture buf=dx.allocator.alloc(w,h,mipCnt,frm);
  Detail::DSharedPtr<Detail::DxTexture*> pbuf(new Detail::DxTexture(std::move(buf)));

  return PTexture(pbuf.handler);
  }

void DirectX12Api::readPixels(Device* d, Pixmap& out, const PTexture t, TextureLayout lay,
                              TextureFormat frm, const uint32_t w, const uint32_t h, uint32_t mip) {

  }

AbstractGraphicsApi::CommandBundle* DirectX12Api::createCommandBuffer(Device* d, FboLayout*) {
  Detail::DxDevice* dx = reinterpret_cast<Detail::DxDevice*>(d);
  return new DxCommandBuffer(*dx);
  }

AbstractGraphicsApi::CommandBuffer* DirectX12Api::createCommandBuffer(Device* d) {
  Detail::DxDevice* dx = reinterpret_cast<Detail::DxDevice*>(d);
  return new DxCommandBuffer(*dx);
  }

void DirectX12Api::present(AbstractGraphicsApi::Device* d, AbstractGraphicsApi::Swapchain* sw,
                           uint32_t imageId, const AbstractGraphicsApi::Semaphore* wait) {
  // TODO: handle imageId
  Detail::DxDevice&    dx    = *reinterpret_cast<Detail::DxDevice*>(d);
  auto&                swait = *reinterpret_cast<const Detail::DxSemaphore*>(wait);
  Detail::DxSwapchain* sx    = reinterpret_cast<Detail::DxSwapchain*>(sw);

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
  //TODO
  caps.setSamplerFormats(-1);
  caps.setAttachFormats(-1);
  }


#endif
