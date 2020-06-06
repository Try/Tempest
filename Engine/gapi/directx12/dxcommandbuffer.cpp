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

#include "guid.h"

#include <cassert>

using namespace Tempest;
using namespace Tempest::Detail;

struct DxCommandBuffer::ImgState {
  ID3D12Resource*       img=nullptr;
  D3D12_RESOURCE_STATES lay;
  D3D12_RESOURCE_STATES last;
  bool                  outdated;
  };

DxCommandBuffer::DxCommandBuffer(DxDevice& d)
  : dev(d) {
  dxAssert(d.device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                            uuid<ID3D12CommandAllocator>(),
                                            reinterpret_cast<void**>(&pool)));

  dxAssert(d.device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, pool.get(), nullptr,
                                       uuid<ID3D12GraphicsCommandList>(), reinterpret_cast<void**>(&impl)));
  impl->Close();
  }

DxCommandBuffer::~DxCommandBuffer() {
  }

void DxCommandBuffer::begin() {
  reset();
  recording = true;
  }

void DxCommandBuffer::end() {
  for(auto& i:imgState)
    i.outdated = true;
  flushLayout();
  imgState.clear();

  dxAssert(impl->Close());
  recording = false;
  resetDone = false;
  }

void DxCommandBuffer::reset() {
  if(resetDone)
    return;
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

  currentFbo = &fbo;

  for(auto& i:imgState)
    i.outdated = true;

  for(size_t i=0;i<fbo.viewsCount;++i) {
    // TODO: STORE op for renderpass
    D3D12_RESOURCE_STATES lay = D3D12_RESOURCE_STATE_RENDER_TARGET;
    const bool preserve = pass.isAttachPreserved(i);
    setLayout(fbo.views[i].res,lay,fbo.views[i].isSwImage,preserve);
    }

  flushLayout();

  auto  desc = fbo.rtvHeap->GetCPUDescriptorHandleForHeapStart();
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
  }

void DxCommandBuffer::endRenderPass() {
  }

void Tempest::Detail::DxCommandBuffer::setPipeline(Tempest::AbstractGraphicsApi::Pipeline& p,
                                                   uint32_t /*w*/, uint32_t /*h*/) {
  DxPipeline& px = reinterpret_cast<DxPipeline&>(p);
  vboStride = px.stride;

  impl->SetPipelineState(&px.instance(*currentFbo));
  impl->SetGraphicsRootSignature(px.sign.get());
  impl->IASetPrimitiveTopology(px.topology);
  }

void DxCommandBuffer::setViewport(const Rect& r) {
  D3D12_VIEWPORT vp={};
  vp.TopLeftX = float(r.x);
  vp.TopLeftY = float(r.y);
  vp.Width    = float(r.w);
  vp.Height   = float(r.h);
  vp.MinDepth = 0.f;
  vp.MaxDepth = 1.f;

  impl->RSSetViewports(1,&vp);
  }

void DxCommandBuffer::setUniforms(AbstractGraphicsApi::Pipeline& /*p*/,
                                  AbstractGraphicsApi::Desc& u,
                                  size_t offc, const uint32_t* /*offv*/) {
  assert(offc==0); //TODO: dynamic offsets
  DxDescriptorArray& ux = reinterpret_cast<DxDescriptorArray&>(u);

  ID3D12DescriptorHeap* ppHeaps[DxUniformsLay::VisTypeCount*DxUniformsLay::MaxPrmPerStage] = {};
  UINT                  heapCount  = 0;
  for(auto& i:ux.heap) {
    if(i.get()==nullptr)
      continue;
    ppHeaps[heapCount] = i.get();
    heapCount++;
    }
  impl->SetDescriptorHeaps(heapCount, ppHeaps);

  for(UINT i=0;i<heapCount;++i)
    impl->SetGraphicsRootDescriptorTable(i, ux.heap[i]->GetGPUDescriptorHandleForHeapStart());
  }

void DxCommandBuffer::exec(const CommandBundle& buf) {
  auto& dx = reinterpret_cast<const DxCommandBuffer&>(buf);
  impl->ExecuteBundle(dx.impl.get()); //BUNDLE!
  }

void DxCommandBuffer::changeLayout(AbstractGraphicsApi::Swapchain& s, uint32_t id, TextureFormat /*frm*/,
                                   TextureLayout prev, TextureLayout next) {
  DxSwapchain&    sw  = reinterpret_cast<DxSwapchain&>(s);
  ID3D12Resource* res = sw.views[id].get();
  static const D3D12_RESOURCE_STATES layouts[] = {
    D3D12_RESOURCE_STATE_COMMON,
    D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
    D3D12_RESOURCE_STATE_RENDER_TARGET,
    D3D12_RESOURCE_STATE_DEPTH_WRITE,
    D3D12_RESOURCE_STATE_PRESENT,
    };
  implChangeLayout(res,D3D12_RESOURCE_STATE_COMMON!=layouts[int(prev)],
                   layouts[int(prev)],layouts[int(next)]);
  }

void DxCommandBuffer::changeLayout(AbstractGraphicsApi::Texture& t, TextureFormat /*frm*/, TextureLayout prev, TextureLayout next) {
  DxTexture& tex = reinterpret_cast<DxTexture&>(t);
  D3D12_RESOURCE_BARRIER barrier = {};
  barrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
  barrier.Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;
  barrier.Transition.pResource   = tex.impl.get();
  barrier.Transition.StateBefore = nativeFormat(prev);
  barrier.Transition.StateAfter  = nativeFormat(next);
  barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
  impl->ResourceBarrier(1, &barrier);
  }

void DxCommandBuffer::changeLayout(AbstractGraphicsApi::Texture& t, TextureFormat /*frm*/, TextureLayout prev, TextureLayout next, uint32_t mipCnt) {
  DxTexture& tex = reinterpret_cast<DxTexture&>(t);
  D3D12_RESOURCE_BARRIER barrier = {};
  barrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
  barrier.Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;
  barrier.Transition.pResource   = tex.impl.get();
  barrier.Transition.StateBefore = nativeFormat(prev);
  barrier.Transition.StateAfter  = nativeFormat(next);
  barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
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
  impl->DrawInstanced(UINT(vertexCount),1,UINT(offset),0);
  }

void DxCommandBuffer::drawIndexed(size_t ioffset, size_t isize, size_t voffset) {
  impl->DrawIndexedInstanced(UINT(isize),1,UINT(ioffset),INT(voffset),0);
  }

void DxCommandBuffer::flush(const DxBuffer&, size_t /*size*/) {
  // NOP
  }

void DxCommandBuffer::copy(DxBuffer& dest, size_t offsetDest, const DxBuffer& src, size_t offsetSrc, size_t size) {
  impl->CopyBufferRegion(dest.impl.get(),offsetDest,src.impl.get(),offsetSrc, size);
  }

void DxCommandBuffer::copy(DxTexture& dest, size_t width, size_t height, size_t mip,
                           const DxBuffer& src, size_t offset) {
  const UINT bpp = dest.bitCount()/8;

  D3D12_PLACED_SUBRESOURCE_FOOTPRINT foot = {};
  foot.Offset             = offset;
  foot.Footprint.Format   = dest.format;
  foot.Footprint.Width    = UINT(width);
  foot.Footprint.Height   = UINT(height);
  foot.Footprint.Depth    = 1;
  foot.Footprint.RowPitch = UINT((width*bpp+D3D12_TEXTURE_DATA_PITCH_ALIGNMENT-1)/D3D12_TEXTURE_DATA_PITCH_ALIGNMENT)*D3D12_TEXTURE_DATA_PITCH_ALIGNMENT;

  D3D12_TEXTURE_COPY_LOCATION dstLoc = {};
  dstLoc.pResource        = dest.impl.get();
  dstLoc.Type             = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
  dstLoc.SubresourceIndex = UINT(mip);

  D3D12_TEXTURE_COPY_LOCATION srcLoc = {};
  srcLoc.pResource        = src.impl.get();
  srcLoc.Type             = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
  srcLoc.PlacedFootprint  = foot;

  impl->CopyTextureRegion(&dstLoc, 0, 0, 0, &srcLoc, nullptr);
  }

void DxCommandBuffer::copy(DxBuffer& dest, size_t width, size_t height, size_t mip,
                           const DxTexture& src, size_t offset) {
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
  dstLoc.pResource        = dest.impl.get();
  dstLoc.Type             = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
  dstLoc.PlacedFootprint  = foot;

  impl->CopyTextureRegion(&dstLoc, 0, 0, 0, &srcLoc, nullptr);
  }

void DxCommandBuffer::generateMipmap(DxTexture& image, TextureFormat imageFormat, uint32_t texWidth, uint32_t texHeight, uint32_t mipLevels) {
  // TODO
  Log::d("TODO: DxCommandBuffer::generateMipmap");
  changeLayout(image, imageFormat, TextureLayout::TransferDest, TextureLayout::Sampler, mipLevels);
  }

void DxCommandBuffer::setLayout(ID3D12Resource* res, D3D12_RESOURCE_STATES lay, bool isSwImage, bool preserve) {
  ImgState* img;
  if(isSwImage) {
    img = &findImg(res,D3D12_RESOURCE_STATE_PRESENT,preserve);
    } else {
    img = &findImg(res,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,preserve);
    }
  implChangeLayout(img->img,preserve,img->lay,lay);
  //img->preserveLast = preserve;
  img->outdated     = false;
  img->lay          = lay;
  }

void DxCommandBuffer::implChangeLayout(ID3D12Resource* res, bool preserveIn,
                                       D3D12_RESOURCE_STATES prev, D3D12_RESOURCE_STATES lay) {
  if(prev==lay)
    return;
  D3D12_RESOURCE_BARRIER barrier = {};
  barrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
  barrier.Flags                  = preserveIn ?
                                   D3D12_RESOURCE_BARRIER_FLAG_NONE : D3D12_RESOURCE_BARRIER_FLAG_END_ONLY;
  barrier.Transition.pResource   = res;
  barrier.Transition.StateBefore = prev;
  barrier.Transition.StateAfter  = lay;
  barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

  impl->ResourceBarrier(1,&barrier);
  }

DxCommandBuffer::ImgState& DxCommandBuffer::findImg(ID3D12Resource* img, D3D12_RESOURCE_STATES last, bool /*preserve*/) {
  for(auto& i:imgState) {
    if(i.img==img)
      return i;
    }
  ImgState s = {};
  s.img  = img;
  s.lay  = last;
  s.last = last;
  imgState.push_back(s);
  return imgState.back();
  }

void DxCommandBuffer::flushLayout() {
  for(auto& i:imgState) {
    if(!i.outdated)
      continue;
    //if(Detail::nativeIsDepthFormat(i.frm))
    //  continue; // no readable depth for now
    implChangeLayout(i.img,true,i.lay,i.last);
    i.lay = i.last;
    }
  }

#endif
