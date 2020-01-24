#include "directx12api.h"

#if defined(TEMPEST_BUILD_DIRECTX12)
#include <dxgi1_6.h>
#include <d3d12.h>

#include "directx12/guid.h"
#include "directx12/comptr.h"
#include "directx12/dxdevice.h"
#include "directx12/dxbuffer.h"

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
  };

DirectX12Api::DirectX12Api(ApiFlags f) {
  impl.reset(new Impl(bool(f&ApiFlags::Validation)));
  }

DirectX12Api::~DirectX12Api(){
  }

AbstractGraphicsApi::Device* DirectX12Api::createDevice() {
  return new DxDevice(*impl->DXGIFactory);
  }

void DirectX12Api::destroy(AbstractGraphicsApi::Device* d) {
  delete d;
  }

AbstractGraphicsApi::Swapchain* DirectX12Api::createSwapchain(SystemApi::Window* w, AbstractGraphicsApi::Device* d) {
  return nullptr;
  }

AbstractGraphicsApi::PPass DirectX12Api::createPass(AbstractGraphicsApi::Device* d, const FboMode** att, size_t acount) {
  return PPass();
  }

AbstractGraphicsApi::PFbo DirectX12Api::createFbo(AbstractGraphicsApi::Device* d, AbstractGraphicsApi::FboLayout* lay,
                                                  AbstractGraphicsApi::Swapchain* s, uint32_t imageId) {
  return PFbo();
  }

AbstractGraphicsApi::PFbo DirectX12Api::createFbo(AbstractGraphicsApi::Device* d, AbstractGraphicsApi::FboLayout* lay, AbstractGraphicsApi::Swapchain* s,
                                                  uint32_t imageId, AbstractGraphicsApi::Texture* zbuf) {
  return PFbo();
  }

AbstractGraphicsApi::PFbo DirectX12Api::createFbo(AbstractGraphicsApi::Device* d, AbstractGraphicsApi::FboLayout* lay, uint32_t w, uint32_t h,
                                                  AbstractGraphicsApi::Texture* cl, AbstractGraphicsApi::Texture* zbuf) {
  return PFbo();
  }

AbstractGraphicsApi::PFbo DirectX12Api::createFbo(AbstractGraphicsApi::Device* d, AbstractGraphicsApi::FboLayout* lay,
                                                  uint32_t w, uint32_t h, AbstractGraphicsApi::Texture* cl) {
  return PFbo();
  }

AbstractGraphicsApi::PFboLayout DirectX12Api::createFboLayout(AbstractGraphicsApi::Device* d, uint32_t w, uint32_t h,
                                                              AbstractGraphicsApi::Swapchain* s, TextureFormat* att, size_t attCount) {
  return PFboLayout();
  }

AbstractGraphicsApi::PPipeline DirectX12Api::createPipeline(AbstractGraphicsApi::Device* d, const RenderState& st,
                                                            const Decl::ComponentType* decl, size_t declSize, size_t stride,
                                                            Topology tp, const UniformsLayout& ulay,
                                                            std::shared_ptr<AbstractGraphicsApi::UniformsLay>& ulayImpl,
                                                            const std::initializer_list<AbstractGraphicsApi::Shader*>& shaders) {
  return PPipeline();
  }

AbstractGraphicsApi::PShader DirectX12Api::createShader(AbstractGraphicsApi::Device* d, const char* source, size_t src_size) {
  return PShader();
  }

AbstractGraphicsApi::Fence* DirectX12Api::createFence(AbstractGraphicsApi::Device* d) {
  return nullptr;
  }

AbstractGraphicsApi::Semaphore* DirectX12Api::createSemaphore(AbstractGraphicsApi::Device* d) {
  return nullptr;
  }

AbstractGraphicsApi::PBuffer DirectX12Api::createBuffer(AbstractGraphicsApi::Device* d, const void* mem,
                                                        size_t count,size_t sz,size_t alignedSz, MemUsage usage, BufferFlags flg) {
  Detail::DxDevice* dx = reinterpret_cast<Detail::DxDevice*>(d);
  (void)sz;

  if(flg==BufferFlags::Dynamic || true) {
    Detail::DxBuffer stage=dx->allocator.alloc(mem,count*alignedSz,usage,BufferFlags::Dynamic);
    return PBuffer(new Detail::DxBuffer(std::move(stage)));
    }
  if(flg==BufferFlags::Staging) {
    Detail::DxBuffer stage=dx->allocator.alloc(mem,count*alignedSz,usage,BufferFlags::Staging);
    return PBuffer(new Detail::DxBuffer(std::move(stage)));
    }/*
  else {
    Detail::DxBuffer  stage=dx->allocator.alloc(mem,     size, MemUsage::TransferSrc,      BufferFlags::Staging);
    Detail::DxBuffer  buf  =dx->allocator.alloc(nullptr, size, usage|MemUsage::TransferDst,BufferFlags::Static );

    Detail::DSharedPtr<Detail::DxBuffer*> pstage(new Detail::DxBuffer(std::move(stage)));
    Detail::DSharedPtr<Detail::DxBuffer*> pbuf  (new Detail::DxBuffer(std::move(buf)));

    DxDevice::Data dat(*dx);
    dat.flush(*pstage.handler,size);
    dat.hold(pbuf);
    dat.hold(pstage); // preserve stage buffer, until gpu side copy is finished
    dat.copy(*pbuf.handler,*pstage.handler,size);
    dat.commit();

    return PBuffer(pbuf.handler);
    }*/
  return PBuffer();
  }

AbstractGraphicsApi::Desc* DirectX12Api::createDescriptors(AbstractGraphicsApi::Device* d, const UniformsLayout& lay, std::shared_ptr<AbstractGraphicsApi::UniformsLay>& layP) {
  return nullptr;
  }

std::shared_ptr<AbstractGraphicsApi::UniformsLay>
DirectX12Api::createUboLayout(AbstractGraphicsApi::Device* d, const UniformsLayout&) {
  return nullptr;
  }

AbstractGraphicsApi::PTexture DirectX12Api::createTexture(AbstractGraphicsApi::Device* d, const Pixmap& p, TextureFormat frm, uint32_t mips) {
  return PTexture();
  }

AbstractGraphicsApi::PTexture DirectX12Api::createTexture(AbstractGraphicsApi::Device* d, const uint32_t w, const uint32_t h, uint32_t mips, TextureFormat frm) {
  return PTexture();
  }

void DirectX12Api::readPixels(AbstractGraphicsApi::Device* d, Pixmap& out, const AbstractGraphicsApi::PTexture t, TextureFormat frm, const uint32_t w, const uint32_t h, uint32_t mip) {

  }

AbstractGraphicsApi::CmdPool* DirectX12Api::createCommandPool(AbstractGraphicsApi::Device* d) {
  return nullptr;
  }

AbstractGraphicsApi::CommandBuffer* DirectX12Api::createCommandBuffer(AbstractGraphicsApi::Device* d, AbstractGraphicsApi::CmdPool* pool,                                                                     AbstractGraphicsApi::FboLayout* fbo, CmdType type) {
  return nullptr;
  }

void DirectX12Api::present(AbstractGraphicsApi::Device* d, AbstractGraphicsApi::Swapchain* sw, uint32_t imageId, const AbstractGraphicsApi::Semaphore* wait) {

  }

void DirectX12Api::draw(AbstractGraphicsApi::Device* d, AbstractGraphicsApi::CommandBuffer* cmd,
                        AbstractGraphicsApi::Semaphore* wait, AbstractGraphicsApi::Semaphore* onReady, AbstractGraphicsApi::Fence* onReadyCpu) {

  }

void DirectX12Api::draw(AbstractGraphicsApi::Device* d, AbstractGraphicsApi::CommandBuffer** cmd, size_t count,
                        AbstractGraphicsApi::Semaphore** wait, size_t waitCnt, AbstractGraphicsApi::Semaphore** done,
                        size_t doneCnt, AbstractGraphicsApi::Fence* doneCpu) {

  }

void DirectX12Api::getCaps(AbstractGraphicsApi::Device* d, AbstractGraphicsApi::Caps& caps) {

  }


#endif
