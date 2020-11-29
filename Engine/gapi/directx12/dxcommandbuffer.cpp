#if defined(TEMPEST_BUILD_DIRECTX12)

#include "dxbuffer.h"
#include "dxtexture.h"
#include "dxcommandbuffer.h"
#include "dxdevice.h"
#include "dxframebuffer.h"
#include "dxpipeline.h"
#include "dxswapchain.h"
#include "dxdescriptorarray.h"
#include "dxrenderpass.h"
#include "dxfbolayout.h"

#include "guid.h"

#include <cassert>

using namespace Tempest;
using namespace Tempest::Detail;

DxCommandBuffer::DxCommandBuffer(DxDevice& d)
  : dev(d) {
  D3D12_COMMAND_LIST_TYPE type = D3D12_COMMAND_LIST_TYPE_DIRECT;
  dxAssert(d.device->CreateCommandAllocator(type,
                                            uuid<ID3D12CommandAllocator>(),
                                            reinterpret_cast<void**>(&pool)));

  dxAssert(d.device->CreateCommandList(0, type, pool.get(), nullptr,
                                       uuid<ID3D12GraphicsCommandList>(), reinterpret_cast<void**>(&impl)));
  impl->Close();
  }

DxCommandBuffer::~DxCommandBuffer() {
  clearStage();
  }

void DxCommandBuffer::begin() {
  reset();
  recording = true;
  }

void DxCommandBuffer::end() {
  if(currentFbo!=nullptr)
    endRenderPass();
  resState.finalize(*this);

  dxAssert(impl->Close());
  recording = false;
  resetDone = false;
  for(size_t i=0;i<DxUniformsLay::MAX_BINDS;++i)
    currentHeaps[i] = nullptr;
  }

void DxCommandBuffer::reset() {
  if(resetDone)
    return;
  clearStage();
  dxAssert(pool->Reset());
  dxAssert(impl->Reset(pool.get(),nullptr));
  resetDone = true;
  }

bool DxCommandBuffer::isRecording() const {
  return recording;
  }

void DxCommandBuffer::beginRenderPass(AbstractGraphicsApi::Fbo*  f,
                                      AbstractGraphicsApi::Pass* p,
                                      uint32_t w, uint32_t h) {
  auto& fbo  = *reinterpret_cast<DxFramebuffer*>(f);
  auto& pass = *reinterpret_cast<DxRenderPass*>(p);

  currentFbo  = &fbo;
  currentPass = &pass;

  for(size_t i=0;i<fbo.viewsCount;++i) {
    const bool preserve = pass.isAttachPreserved(i);
    resState.setLayout(fbo.views[i],fbo.views[i].renderLayout(),preserve);
    }
  if(fbo.depth.res!=nullptr) {
    const bool preserve = pass.isAttachPreserved(pass.att.size()-1);
    resState.setLayout(fbo.depth,fbo.depth.renderLayout(),preserve);
    }

  resState.flushLayout(*this);

  auto desc = fbo.rtvHeap->GetCPUDescriptorHandleForHeapStart();
  if(fbo.depth.res!=nullptr) {
    auto ds = fbo.dsvHeap->GetCPUDescriptorHandleForHeapStart();
    impl->OMSetRenderTargets(fbo.viewsCount, &desc, TRUE, &ds);
    } else {
    impl->OMSetRenderTargets(fbo.viewsCount, &desc, TRUE, nullptr);
    }

  D3D12_VIEWPORT vp={};
  vp.TopLeftX = float(0.f);
  vp.TopLeftY = float(0.f);
  vp.Width    = float(w);
  vp.Height   = float(h);
  vp.MinDepth = 0.f;
  vp.MaxDepth = 1.f;
  impl->RSSetViewports(1, &vp);

  D3D12_RECT sr={};
  sr.left   = 0;
  sr.top    = 0;
  sr.right  = LONG(w);
  sr.bottom = LONG(h);
  impl->RSSetScissorRects(1, &sr);

  for(size_t i=0;i<fbo.viewsCount;++i) {
    auto& att = pass.att[i];
    if(FboMode::ClearBit!=(att.mode&FboMode::ClearBit))
      continue;
    const float clearColor[] = { att.clear.r(), att.clear.g(), att.clear.b(), att.clear.a() };
    impl->ClearRenderTargetView(desc, clearColor, 0, nullptr);
    desc.ptr+=fbo.rtvHeapInc;
    }

  if(fbo.depth.res!=nullptr) {
    auto& att = pass.att.back();
    if(FboMode::ClearBit==(att.mode&FboMode::ClearBit)) {
      auto ds = fbo.dsvHeap->GetCPUDescriptorHandleForHeapStart();
      impl->ClearDepthStencilView(ds,D3D12_CLEAR_FLAG_DEPTH,att.clear.r(),0, 0,nullptr);
      }
    }
  }

void DxCommandBuffer::endRenderPass() {
  auto& fbo  = *currentFbo;
  auto& pass = *currentPass;

  for(size_t i=0;i<fbo.viewsCount;++i) {
    const bool preserve = pass.isResultPreserved(i);
    resState.setLayout(fbo.views[i],fbo.views[i].defaultLayout(),preserve);
    }

  if(fbo.depth.res!=nullptr) {
    const bool preserve = pass.isResultPreserved(pass.att.size()-1);
    resState.setLayout(fbo.depth,fbo.depth.defaultLayout(),preserve);
    }

  currentFbo  = nullptr;
  currentPass = nullptr;
  }

void DxCommandBuffer::setViewport(const Rect& r) {
  D3D12_VIEWPORT vp={};
  vp.TopLeftX = float(r.x);
  vp.TopLeftY = float(r.y);
  vp.Width    = float(r.w);
  vp.Height   = float(r.h);
  vp.MinDepth = 0.f;
  vp.MaxDepth = 1.f;

  impl->RSSetViewports(1, &vp);
  }

void Tempest::Detail::DxCommandBuffer::setPipeline(Tempest::AbstractGraphicsApi::Pipeline& p,
                                                   uint32_t /*w*/, uint32_t /*h*/) {
  DxPipeline& px = reinterpret_cast<DxPipeline&>(p);
  vboStride = px.stride;

  impl->SetPipelineState(&px.instance(*currentFbo->lay.handler));
  impl->SetGraphicsRootSignature(px.sign.get());
  impl->IASetPrimitiveTopology(px.topology);
  }

void DxCommandBuffer::setBytes(AbstractGraphicsApi::Pipeline& p, const void* data, size_t size) {
  auto& px = reinterpret_cast<DxPipeline&>(p);
  impl->SetGraphicsRoot32BitConstants(UINT(px.pushConstantId),UINT(size/4),data,0);
  }

void DxCommandBuffer::setUniforms(AbstractGraphicsApi::Pipeline& /*p*/, AbstractGraphicsApi::Desc& u) {
  implSetUniforms(u,false);
  }

void Tempest::Detail::DxCommandBuffer::setComputePipeline(Tempest::AbstractGraphicsApi::CompPipeline& p) {
  auto& px = reinterpret_cast<DxCompPipeline&>(p);
  impl->SetPipelineState(px.impl.get());
  impl->SetComputeRootSignature(px.sign.get());
  }

void DxCommandBuffer::setBytes(AbstractGraphicsApi::CompPipeline& p, const void* data, size_t size) {
  auto& px = reinterpret_cast<DxCompPipeline&>(p);
  impl->SetComputeRoot32BitConstants(UINT(px.pushConstantId),UINT(size/4),data,0);
  }

void DxCommandBuffer::setUniforms(AbstractGraphicsApi::CompPipeline& /*p*/, AbstractGraphicsApi::Desc& u) {
  implSetUniforms(u,true);
  }

void DxCommandBuffer::implSetUniforms(AbstractGraphicsApi::Desc& u, bool isCompute) {
  DxDescriptorArray& ux = reinterpret_cast<DxDescriptorArray&>(u);

  bool setH = false;
  for(size_t i=0;i<DxUniformsLay::MAX_BINDS;++i) {
    if(ux.val.heap[i]!=currentHeaps[i]) {
      setH = true;
      break;
      }
    }

  if(setH) {
    for(size_t i=0;i<DxUniformsLay::MAX_BINDS;++i)
      currentHeaps[i] = ux.val.heap[i];
    impl->SetDescriptorHeaps(ux.heapCnt, currentHeaps);
    }

  auto& lx = *ux.layPtr.handler;
  for(size_t i=0;i<lx.roots.size();++i) {
    auto& r   = lx.roots[i];
    auto desc = ux.val.gpu[r.heap];
    desc.ptr+=r.heapOffset;
    if(isCompute)
      impl->SetComputeRootDescriptorTable(UINT(i), desc); else
      impl->SetGraphicsRootDescriptorTable(UINT(i), desc);
    }
  }

void DxCommandBuffer::changeLayout(AbstractGraphicsApi::Buffer& buf, BufferLayout prev, BufferLayout next) {
  // TODO
  }

void DxCommandBuffer::changeLayout(AbstractGraphicsApi::Attach& att, TextureLayout prev, TextureLayout next, bool /*byRegion*/) {
  auto& img = reinterpret_cast<DxFramebuffer::Attach&>(att);
  if((prev==TextureLayout::ColorAttach || prev==TextureLayout::DepthAttach) && next==TextureLayout::Undefined)
    impl->DiscardResource(img.res,nullptr);

  auto p = (prev==TextureLayout::Undefined ? att.defaultLayout() : prev);
  auto n = (next==TextureLayout::Undefined ? att.defaultLayout() : next);
  if(n!=p)
    implChangeLayout(img.res,nativeFormat(p),nativeFormat(n));

  if(prev==TextureLayout::Undefined && (next==TextureLayout::ColorAttach || next==TextureLayout::DepthAttach))
    impl->DiscardResource(img.res,nullptr);
  }

void DxCommandBuffer::changeLayout(AbstractGraphicsApi::Texture& t,
                                   TextureLayout prev, TextureLayout next, uint32_t mipId) {
  DxTexture& tex = reinterpret_cast<DxTexture&>(t);
  D3D12_RESOURCE_BARRIER barrier = {};
  barrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
  barrier.Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;
  barrier.Transition.pResource   = tex.impl.get();
  barrier.Transition.StateBefore = nativeFormat(prev);
  barrier.Transition.StateAfter  = nativeFormat(next);
  barrier.Transition.Subresource = (mipId==uint32_t(-1) ?  D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES : mipId);
  impl->ResourceBarrier(1, &barrier);
  }

void DxCommandBuffer::setVbo(const AbstractGraphicsApi::Buffer& b) {
  const DxBuffer& bx = reinterpret_cast<const DxBuffer&>(b);

  D3D12_VERTEX_BUFFER_VIEW view;
  view.BufferLocation = bx.impl.get()->GetGPUVirtualAddress();
  view.SizeInBytes    = bx.sizeInBytes;
  view.StrideInBytes  = vboStride;
  impl->IASetVertexBuffers(0,1,&view);
  }

void DxCommandBuffer::setIbo(const AbstractGraphicsApi::Buffer& b, IndexClass cls) {
  const DxBuffer& bx = reinterpret_cast<const DxBuffer&>(b);
  static const DXGI_FORMAT type[]={
    DXGI_FORMAT_R16_UINT,
    DXGI_FORMAT_R32_UINT
    };

  D3D12_INDEX_BUFFER_VIEW view;
  view.BufferLocation = bx.impl.get()->GetGPUVirtualAddress();
  view.SizeInBytes    = bx.sizeInBytes;
  view.Format         = type[uint32_t(cls)];
  impl->IASetIndexBuffer(&view);
  }

void DxCommandBuffer::draw(size_t offset, size_t vertexCount) {
  if(currentFbo==nullptr)
    throw std::system_error(Tempest::GraphicsErrc::DrawCallWithoutFbo);
  impl->DrawInstanced(UINT(vertexCount),1,UINT(offset),0);
  }

void DxCommandBuffer::drawIndexed(size_t ioffset, size_t isize, size_t voffset) {
  if(currentFbo==nullptr)
    throw std::system_error(Tempest::GraphicsErrc::DrawCallWithoutFbo);
  impl->DrawIndexedInstanced(UINT(isize),1,UINT(ioffset),INT(voffset),0);
  }

void DxCommandBuffer::dispatch(size_t x, size_t y, size_t z) {
  if(currentFbo!=nullptr)
    throw std::system_error(Tempest::GraphicsErrc::ComputeCallInRenderPass);
  resState.flushLayout(*this);
  impl->Dispatch(UINT(x),UINT(y),UINT(z));
  }

void DxCommandBuffer::copy(AbstractGraphicsApi::Buffer& dstBuf, size_t offsetDest, const AbstractGraphicsApi::Buffer& srcBuf, size_t offsetSrc, size_t size) {
  auto& dst = reinterpret_cast<DxBuffer&>(dstBuf);
  auto& src = reinterpret_cast<const DxBuffer&>(srcBuf);
  impl->CopyBufferRegion(dst.impl.get(),offsetDest,src.impl.get(),offsetSrc,size);
  }

void DxCommandBuffer::copy(AbstractGraphicsApi::Texture& dstTex, size_t width, size_t height, size_t mip,
                           const AbstractGraphicsApi::Buffer& srcBuf, size_t offset) {
  auto& dst = reinterpret_cast<DxTexture&>(dstTex);
  auto& src = reinterpret_cast<const DxBuffer&>(srcBuf);

  D3D12_PLACED_SUBRESOURCE_FOOTPRINT foot = {};
  foot.Offset             = offset;
  foot.Footprint.Format   = dst.format;
  foot.Footprint.Width    = UINT(width);
  foot.Footprint.Height   = UINT(height);
  foot.Footprint.Depth    = 1;
  if(dst.format==DXGI_FORMAT_BC1_UNORM ||
     dst.format==DXGI_FORMAT_BC2_UNORM ||
     dst.format==DXGI_FORMAT_BC3_UNORM) {
    foot.Footprint.RowPitch = UINT((width+3)/4)*(dst.format==DXGI_FORMAT_BC1_UNORM ? 8 : 16);
    foot.Footprint.RowPitch = ((foot.Footprint.RowPitch+D3D12_TEXTURE_DATA_PITCH_ALIGNMENT-1)
                               /D3D12_TEXTURE_DATA_PITCH_ALIGNMENT)*D3D12_TEXTURE_DATA_PITCH_ALIGNMENT;
    } else {
    const UINT bpp = dst.bitCount()/8;
    foot.Footprint.RowPitch = UINT((width*bpp+D3D12_TEXTURE_DATA_PITCH_ALIGNMENT-1)
                                   /D3D12_TEXTURE_DATA_PITCH_ALIGNMENT)*D3D12_TEXTURE_DATA_PITCH_ALIGNMENT;
    }

  D3D12_TEXTURE_COPY_LOCATION dstLoc = {};
  dstLoc.pResource        = dst.impl.get();
  dstLoc.Type             = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
  dstLoc.SubresourceIndex = UINT(mip);

  D3D12_TEXTURE_COPY_LOCATION srcLoc = {};
  srcLoc.pResource        = src.impl.get();
  srcLoc.Type             = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
  srcLoc.PlacedFootprint  = foot;

  impl->CopyTextureRegion(&dstLoc, 0, 0, 0, &srcLoc, nullptr);
  }

void DxCommandBuffer::copy(AbstractGraphicsApi::Buffer& dstBuf, size_t width, size_t height, size_t mip,
                           const AbstractGraphicsApi::Texture& srcTex, size_t offset) {
  auto& dst = reinterpret_cast<DxBuffer&>(dstBuf);
  auto& src = reinterpret_cast<const DxTexture&>(srcTex);

  const UINT bpp = src.bitCount()/8;

  D3D12_PLACED_SUBRESOURCE_FOOTPRINT foot = {};
  foot.Offset             = offset;
  foot.Footprint.Format   = src.format;
  foot.Footprint.Width    = UINT(width);
  foot.Footprint.Height   = UINT(height);
  foot.Footprint.Depth    = 1;
  foot.Footprint.RowPitch = UINT((width*bpp+D3D12_TEXTURE_DATA_PITCH_ALIGNMENT-1)/D3D12_TEXTURE_DATA_PITCH_ALIGNMENT)*D3D12_TEXTURE_DATA_PITCH_ALIGNMENT;

  D3D12_TEXTURE_COPY_LOCATION srcLoc = {};
  srcLoc.pResource        = src.impl.get();
  srcLoc.Type             = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
  srcLoc.SubresourceIndex = UINT(mip);

  D3D12_TEXTURE_COPY_LOCATION dstLoc = {};
  dstLoc.pResource        = dst.impl.get();
  dstLoc.Type             = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
  dstLoc.PlacedFootprint  = foot;

  impl->CopyTextureRegion(&dstLoc, 0, 0, 0, &srcLoc, nullptr);
  }

void DxCommandBuffer::clearStage() {
  while(stageResources!=nullptr) {
    auto s = stageResources;
    stageResources = stageResources->next;
    delete s;
    }
  }

void DxCommandBuffer::blitFS(AbstractGraphicsApi::Texture& srcTex, uint32_t srcW, uint32_t srcH, uint32_t srcMip,
                             AbstractGraphicsApi::Texture& dstTex, uint32_t dstW, uint32_t dstH, uint32_t dstMip) {
  (void)srcW;
  (void)srcH;

  struct Blit : Stage {
    Blit(DxDevice& dev,DxUniformsLay& lay, DxTexture& dest, size_t dstMip):desc(dev,lay) {
      // descriptor heap
      D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
      rtvHeapDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
      rtvHeapDesc.NumDescriptors = UINT(1);
      rtvHeapDesc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
      dxAssert(dev.device->CreateDescriptorHeap(&rtvHeapDesc, uuid<ID3D12DescriptorHeap>(), reinterpret_cast<void**>(&rtvHeap)));

      rtvHandle = rtvHeap->GetCPUDescriptorHandleForHeapStart();

      D3D12_RENDER_TARGET_VIEW_DESC view = {};
      view.Format             = dest.format;
      view.ViewDimension      = D3D12_RTV_DIMENSION_TEXTURE2D;
      view.Texture2D.MipSlice = UINT(dstMip);

      dev.device->CreateRenderTargetView(dest.impl.get(), &view, rtvHandle);
      }

    ComPtr<ID3D12DescriptorHeap> rtvHeap;
    D3D12_CPU_DESCRIPTOR_HANDLE  rtvHandle;
    DxDescriptorArray            desc;
    };

  const DXGI_FORMAT frm = reinterpret_cast<DxTexture&>(srcTex).format;
  std::unique_ptr<Blit> dx(new Blit(dev,*dev.blitLayout.handler,reinterpret_cast<DxTexture&>(dstTex),dstMip));
  impl->OMSetRenderTargets(1, &dx->rtvHandle, TRUE, nullptr);

  D3D12_VIEWPORT vp={};
  vp.TopLeftX = float(0.f);
  vp.TopLeftY = float(0.f);
  vp.Width    = float(dstW);
  vp.Height   = float(dstH);
  vp.MinDepth = 0.f;
  vp.MaxDepth = 1.f;
  impl->RSSetViewports(1, &vp);

  D3D12_RECT sr={};
  sr.left   = 0;
  sr.top    = 0;
  sr.right  = LONG(dstW);
  sr.bottom = LONG(dstH);
  impl->RSSetScissorRects(1, &sr);

  dx->desc.set(0,&srcTex,srcMip,Sampler2d::bilinear());

  auto& shader = *dev.blit.handler;
  impl->SetPipelineState(&shader.instance(frm));
  impl->SetGraphicsRootSignature(shader.sign.get());
  impl->IASetPrimitiveTopology(shader.topology);
  implSetUniforms(dx->desc,false);

  D3D12_VERTEX_BUFFER_VIEW view;
  view.BufferLocation = dev.vboFsq.handler->impl.get()->GetGPUVirtualAddress();
  view.SizeInBytes    = dev.vboFsq.handler->sizeInBytes;
  view.StrideInBytes  = shader.stride;
  impl->IASetVertexBuffers(0,1,&view);

  impl->DrawInstanced(6,1,0,0);

  dx->next       = stageResources;
  stageResources = dx.release();
  }

void DxCommandBuffer::blitCS(AbstractGraphicsApi::Texture& srcTex, uint32_t srcW, uint32_t srcH, uint32_t srcMip,
                             AbstractGraphicsApi::Texture& dstTex, uint32_t dstW, uint32_t dstH, uint32_t dstMip) {
  (void)srcW;
  (void)srcH;

  const DXGI_FORMAT frm     = reinterpret_cast<DxTexture&>(srcTex).format;
  DxCompPipeline*   pshader = nullptr;
  switch(frm) {
    case DXGI_FORMAT_R8_UNORM:
    case DXGI_FORMAT_R8G8_UNORM:
    case DXGI_FORMAT_R8G8B8A8_UNORM:
      pshader = dev.blitRgba8.handler;
      break;
    case DXGI_FORMAT_R32_FLOAT:
      pshader = dev.blitR32f.handler;
      break;
    case DXGI_FORMAT_R32G32_FLOAT:
    case DXGI_FORMAT_R32G32B32_FLOAT:
    case DXGI_FORMAT_R32G32B32A32_FLOAT:
      pshader = dev.blitRgba32f.handler;
      break;
    default:
      throw std::runtime_error("not implemented");
    }
  auto& shader = *pshader;

  struct Blit : Stage {
    Blit(DxDevice& dev,DxUniformsLay& lay):desc(dev,lay) {}
    DxDescriptorArray desc;
    };

  std::unique_ptr<Blit> dx(new Blit(dev,*dev.blitLayoutCs.handler));
  dx->desc.setSsbo(0,&dstTex,dstMip);
  dx->desc.setSsbo(1,&srcTex,srcMip);

  impl->SetPipelineState(shader.impl.get());
  impl->SetComputeRootSignature(shader.sign.get());
  implSetUniforms(dx->desc,true);
  impl->Dispatch(dstW,dstH,1);

  // impl->CopyTextureRegion();

  dx->next       = stageResources;
  stageResources = dx.release();
  }

void DxCommandBuffer::generateMipmap(AbstractGraphicsApi::Texture& img, TextureLayout defLayout,
                                     uint32_t texWidth, uint32_t texHeight, uint32_t mipLevels) {
  if(currentFbo!=nullptr)
    throw std::system_error(Tempest::GraphicsErrc::ComputeCallInRenderPass);

  resState.flushLayout(*this);

  auto& image = reinterpret_cast<const DxTexture&>(img);
  TextureLayout dFrm  = TextureLayout::ColorAttach;
  TextureLayout sFrm  = TextureLayout::Sampler;
  bool          useFS = true;
  if(image.format==DXGI_FORMAT_R32_FLOAT          ||
     image.format==DXGI_FORMAT_R32G32_FLOAT       ||
     image.format==DXGI_FORMAT_R32G32B32_FLOAT    ||
     image.format==DXGI_FORMAT_R32G32B32A32_FLOAT ||
     true) {
    dFrm  = TextureLayout::TransferDest;
    sFrm  = TextureLayout::TransferSrc;
    useFS = false;
    }

  int32_t w = int32_t(texWidth);
  int32_t h = int32_t(texHeight);

  if(defLayout!=dFrm)
    changeLayout(img,defLayout,dFrm,uint32_t(-1));

  for(uint32_t i=1; i<mipLevels; ++i) {
    const int mw = (w==1 ? 1 : w/2);
    const int mh = (h==1 ? 1 : h/2);

    changeLayout(img,dFrm,sFrm,i-1);

    if(useFS) {
      blitFS(img,  w, h, i-1,
             img, mw,mh, i);
      } else {
      blitCS(img,  w, h, i-1,
             img, mw,mh, i);
      }

    if(sFrm!=TextureLayout::Sampler)
      changeLayout(img,sFrm, TextureLayout::Sampler, i-1);

    w = mw;
    h = mh;
    }
  if(dFrm!=TextureLayout::Sampler)
    changeLayout(img,dFrm, TextureLayout::Sampler, mipLevels-1);
  }

void DxCommandBuffer::implChangeLayout(ID3D12Resource* res, D3D12_RESOURCE_STATES prev, D3D12_RESOURCE_STATES lay) {
  D3D12_RESOURCE_BARRIER barrier = {};
  barrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
  barrier.Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;
  barrier.Transition.pResource   = res;
  barrier.Transition.StateBefore = prev;
  barrier.Transition.StateAfter  = lay;
  barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

  impl->ResourceBarrier(1,&barrier);
  }

#endif

