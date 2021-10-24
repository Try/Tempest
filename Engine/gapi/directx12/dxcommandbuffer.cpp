#if defined(TEMPEST_BUILD_DIRECTX12)

#include "dxcommandbuffer.h"
#include "dxbuffer.h"
#include "dxtexture.h"
#include "dxdevice.h"
#include "dxswapchain.h"
#include "dxdescriptorarray.h"
#include "dxfbolayout.h"
#include "dxpipeline.h"

#include "guid.h"

#include <cassert>

using namespace Tempest;
using namespace Tempest::Detail;

struct DxCommandBuffer::Blit : Stage {
  Blit(DxDevice& dev,
       DxTexture& src, uint32_t /*srcW*/, uint32_t /*srcH*/, uint32_t srcMip,
       DxTexture& dst, uint32_t dstW, uint32_t dstH, uint32_t dstMip)
    :src(src), srcMip(srcMip), dstW(dstW), dstH(dstH), desc(*dev.blitLayout.handler) {
    // descriptor heap
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
    rtvHeapDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.NumDescriptors = UINT(1);
    rtvHeapDesc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    dxAssert(dev.device->CreateDescriptorHeap(&rtvHeapDesc, uuid<ID3D12DescriptorHeap>(), reinterpret_cast<void**>(&rtvHeap)));

    rtvHandle = rtvHeap->GetCPUDescriptorHandleForHeapStart();

    D3D12_RENDER_TARGET_VIEW_DESC view = {};
    view.Format             = dst.format;
    view.ViewDimension      = D3D12_RTV_DIMENSION_TEXTURE2D;
    view.Texture2D.MipSlice = UINT(dstMip);

    dev.device->CreateRenderTargetView(dst.impl.get(), &view, rtvHandle);
    }

  void exec(DxCommandBuffer& cmd) override {
    auto& impl = *cmd.impl;
    auto& dev  = cmd.dev;

    const DXGI_FORMAT frm = src.format;
    impl.OMSetRenderTargets(1, &rtvHandle, TRUE, nullptr);

    D3D12_VIEWPORT vp={};
    vp.TopLeftX = float(0.f);
    vp.TopLeftY = float(0.f);
    vp.Width    = float(dstW);
    vp.Height   = float(dstH);
    vp.MinDepth = 0.f;
    vp.MaxDepth = 1.f;
    impl.RSSetViewports(1, &vp);

    D3D12_RECT sr={};
    sr.left   = 0;
    sr.top    = 0;
    sr.right  = LONG(dstW);
    sr.bottom = LONG(dstH);
    impl.RSSetScissorRects(1, &sr);

    desc.set(0,&src,srcMip,Sampler2d::bilinear(),src.format);

    auto& shader = *dev.blit.handler;
    impl.SetPipelineState(&shader.instance(frm));
    impl.SetGraphicsRootSignature(shader.sign.get());
    impl.IASetPrimitiveTopology(shader.topology);
    cmd.implSetUniforms(desc,false);

    impl.IASetVertexBuffers(0,0,nullptr);
    impl.DrawInstanced(6,1,0,0);
    };

  DxTexture&                   src;
  uint32_t                     srcMip;
  uint32_t                     dstW;
  uint32_t                     dstH;

  ComPtr<ID3D12DescriptorHeap> rtvHeap;
  D3D12_CPU_DESCRIPTOR_HANDLE  rtvHandle;
  DxDescriptorArray            desc;
  };

struct DxCommandBuffer::MipMaps : Stage {
  MipMaps(DxDevice& dev, DxTexture& image, ResourceLayout defLayout, uint32_t texW, uint32_t texH, uint32_t mipLevels)
    :img(image),defLayout(defLayout),texW(texW),texH(texH),mipLevels(mipLevels) {
    // descriptor heap
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
    rtvHeapDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.NumDescriptors = UINT(mipLevels-1);
    rtvHeapDesc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    dxAssert(dev.device->CreateDescriptorHeap(&rtvHeapDesc, uuid<ID3D12DescriptorHeap>(), reinterpret_cast<void**>(&rtvHeap)));

    auto                        rtvHeapInc = dev.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle  = rtvHeap->GetCPUDescriptorHandleForHeapStart();

    D3D12_RENDER_TARGET_VIEW_DESC view = {};
    view.Format             = image.format;
    view.ViewDimension      = D3D12_RTV_DIMENSION_TEXTURE2D;
    for(uint32_t i=1; i<mipLevels; ++i) {
      view.Texture2D.MipSlice = UINT(i); //destMip
      dev.device->CreateRenderTargetView(image.impl.get(), &view, rtvHandle);
      rtvHandle.ptr+=rtvHeapInc;
      }
    desc.reserve(mipLevels);
    }

  void blit(DxCommandBuffer& cmd, uint32_t srcMip, uint32_t dstW, uint32_t dstH) {
    auto& impl = *cmd.impl;
    auto& dev  = cmd.dev;

    D3D12_VIEWPORT vp={};
    vp.TopLeftX = float(0.f);
    vp.TopLeftY = float(0.f);
    vp.Width    = float(dstW);
    vp.Height   = float(dstH);
    vp.MinDepth = 0.f;
    vp.MaxDepth = 1.f;
    impl.RSSetViewports(1, &vp);

    D3D12_RECT sr={};
    sr.left   = 0;
    sr.top    = 0;
    sr.right  = LONG(dstW);
    sr.bottom = LONG(dstH);
    impl.RSSetScissorRects(1, &sr);

    desc.emplace_back(*dev.blitLayout.handler);
    DxDescriptorArray& ubo = this->desc.back();
    ubo.set(0,&img,srcMip,Sampler2d::bilinear(),img.format);
    cmd.implSetUniforms(ubo,false);

    impl.DrawInstanced(6,1,0,0);
    }

  void exec(DxCommandBuffer& cmd) override {
    auto& impl = *cmd.impl;
    auto& dev  = cmd.dev;

    int32_t w = int32_t(texW);
    int32_t h = int32_t(texH);

    auto& shader = *dev.blit.handler;
    impl.SetPipelineState(&shader.instance(img.format));
    impl.SetGraphicsRootSignature(shader.sign.get());
    impl.IASetPrimitiveTopology(shader.topology);

    impl.IASetVertexBuffers(0,0,nullptr);

    if(defLayout!=ResourceLayout::ColorAttach)
      cmd.changeLayout(img,defLayout,ResourceLayout::ColorAttach,uint32_t(-1));

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle  = rtvHeap->GetCPUDescriptorHandleForHeapStart();
    auto                        rtvHeapInc = dev.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    for(uint32_t i=1; i<mipLevels; ++i) {
      const int mw = (w==1 ? 1 : w/2);
      const int mh = (h==1 ? 1 : h/2);

      cmd.changeLayout(img,ResourceLayout::ColorAttach,ResourceLayout::Sampler,i-1);
      impl.OMSetRenderTargets(1, &rtvHandle, TRUE, nullptr);
      blit(cmd,i-1,mw,mh);

      w             = mw;
      h             = mh;
      rtvHandle.ptr+= rtvHeapInc;
      }
    cmd.changeLayout(img, ResourceLayout::ColorAttach, ResourceLayout::Sampler, mipLevels-1);
    }

  DxTexture&                     img;
  ResourceLayout                 defLayout;
  uint32_t                       texW;
  uint32_t                       texH;
  uint32_t                       mipLevels;

  ComPtr<ID3D12DescriptorHeap>   rtvHeap;
  std::vector<DxDescriptorArray> desc;
  };

struct DxCommandBuffer::CopyBuf : Stage {
  CopyBuf(DxDevice& dev,
          DxBuffer& dst, size_t offset,
          const DxTexture& src, size_t width, size_t height, size_t mip)
    :dst(dst), offset(offset), src(src), width(width), height(height), mip(int32_t(mip)), desc(*dev.copyLayout.handler) {
    }

  void exec(DxCommandBuffer& cmd) override {
    auto& impl = *cmd.impl;

    struct PushUbo {
      int32_t mip     = 0;
      int32_t bitCnt  = 8;
      int32_t compCnt = 4;
      } push;
    push.mip = mip;

    auto&  prog    = shader(cmd,push.bitCnt,push.compCnt);
    size_t outSize = (width*height*(push.compCnt*push.bitCnt/8) + sizeof(uint32_t)-1)/sizeof(uint32_t);

    desc.set    (0,&src,0,Sampler2d::nearest(),src.format);
    desc.setSsbo(1,&dst,0);

    impl.SetPipelineState(prog.impl.get());
    impl.SetComputeRootSignature(prog.sign.get());
    cmd.implSetUniforms(desc,true);

    impl.SetComputeRoot32BitConstants(UINT(prog.pushConstantId),UINT(sizeof(push)/4),&push,0);
    const size_t maxWG = 65535;
    impl.Dispatch(UINT(std::min(outSize,maxWG)),UINT((outSize+maxWG-1)%maxWG),1u);
    }

  DxCompPipeline& shader(DxCommandBuffer& cmd, int32_t& bitCnt, int32_t& compCnt) {
    auto& dev = cmd.dev;
    switch(src.format) {
      case DXGI_FORMAT_R8_TYPELESS:
      case DXGI_FORMAT_R8_UNORM:
      case DXGI_FORMAT_R8_UINT:
      case DXGI_FORMAT_R8_SNORM:
      case DXGI_FORMAT_R8_SINT:
        bitCnt  = 8;
        compCnt = 1;
        return *dev.copyS.handler;
      case DXGI_FORMAT_R8G8_TYPELESS:
      case DXGI_FORMAT_R8G8_UNORM:
      case DXGI_FORMAT_R8G8_UINT:
      case DXGI_FORMAT_R8G8_SNORM:
      case DXGI_FORMAT_R8G8_SINT:
        bitCnt  = 8;
        compCnt = 2;
        return *dev.copyS.handler;
      case DXGI_FORMAT_R8G8B8A8_TYPELESS:
      case DXGI_FORMAT_R8G8B8A8_UNORM:
      case DXGI_FORMAT_R8G8B8A8_UINT:
      case DXGI_FORMAT_R8G8B8A8_SNORM:
      case DXGI_FORMAT_R8G8B8A8_SINT:
        bitCnt  = 8;
        compCnt = 4;
        return *dev.copy.handler;
      case DXGI_FORMAT_R16_TYPELESS:
      case DXGI_FORMAT_R16_UNORM:
      case DXGI_FORMAT_R16_UINT:
      case DXGI_FORMAT_R16_SNORM:
      case DXGI_FORMAT_R16_SINT:
        bitCnt  = 16;
        compCnt = 1;
        return *dev.copyS.handler;
      case DXGI_FORMAT_R16G16_TYPELESS:
      case DXGI_FORMAT_R16G16_UNORM:
      case DXGI_FORMAT_R16G16_UINT:
      case DXGI_FORMAT_R16G16_SNORM:
      case DXGI_FORMAT_R16G16_SINT:
        bitCnt  = 16;
        compCnt = 2;
        return *dev.copyS.handler;
      case DXGI_FORMAT_R16G16B16A16_TYPELESS:
      case DXGI_FORMAT_R16G16B16A16_UNORM:
      case DXGI_FORMAT_R16G16B16A16_UINT:
      case DXGI_FORMAT_R16G16B16A16_SNORM:
      case DXGI_FORMAT_R16G16B16A16_SINT:
        bitCnt  = 16;
        compCnt = 4;
        return *dev.copyS.handler;
      default:
        dxAssert(E_NOTIMPL);
      }
    return *dev.copy.handler;
    }

  DxBuffer&         dst;
  const size_t      offset = 0;
  const DxTexture&  src;
  const size_t      width  = 0;
  const size_t      height = 0;
  const int32_t     mip    = 0;

  DxDescriptorArray desc;
  };

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
  state = Idle;
  }

void DxCommandBuffer::end() {
  resState.finalize(*this);

  dxAssert(impl->Close());
  state     = NoRecording;
  resetDone = false;
  for(size_t i=0;i<DxPipelineLay::MAX_BINDS;++i)
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
  return state!=NoRecording;
  }

void Tempest::Detail::DxCommandBuffer::beginRendering(const AttachmentDesc* desc, size_t descSize, uint32_t w, uint32_t h,
                                                      const TextureFormat* frm, AbstractGraphicsApi::Texture** att,
                                                      AbstractGraphicsApi::Swapchain** sw, const uint32_t* imgId) {
  for(size_t i=0; i<descSize; ++i) {
    if(isDepthFormat(frm[i]))
      resState.setLayout(*att[i],ResourceLayout::DepthAttach,desc[i].load==AccessOp::Preserve);
    else if(frm[i]==TextureFormat::Undefined)
      resState.setLayout(*sw[i],imgId[i],ResourceLayout::ColorAttach,desc[i].load==AccessOp::Preserve);
    else
      resState.setLayout(*att[i],ResourceLayout::ColorAttach,desc[i].load==AccessOp::Preserve);
    }
  resState.flush(*this);
  for(size_t i=0; i<descSize; ++i) {
    if(isDepthFormat(frm[i]))
      resState.setLayout(*att[i],ResourceLayout::DepthAttach,desc[i].store==AccessOp::Preserve);
    else if(frm[i]==TextureFormat::Undefined)
      resState.setLayout(*sw[i],imgId[i],ResourceLayout::Present,desc[i].store==AccessOp::Preserve);
    else
      resState.setLayout(*att[i],ResourceLayout::Sampler,desc[i].store==AccessOp::Preserve);
    }

  // TODO: render-pass api
  // impl->BeginRenderPass(1, &renderPassRenderTargetDesc, &renderPassDepthStencilDesc, D3D12_RENDER_PASS_FLAG_NONE);

  D3D12_CPU_DESCRIPTOR_HANDLE  view[MaxFramebufferAttachments] = {};
  D3D12_CPU_DESCRIPTOR_HANDLE* ds     = nullptr;
  UINT                         viewSz = 0;
  for(size_t i=0; i<descSize; ++i) {
    if(sw[i]!=nullptr) {
      auto& t                      = *reinterpret_cast<DxSwapchain*>(sw[i]);
      view[viewSz]                 = t.handles[imgId[i]];
      fboLayout.RTVFormats[viewSz] = t.format();
      ++viewSz;
      }
    else if(desc[i].attachment!=nullptr) {
      auto& t                      = *reinterpret_cast<DxTextureWithRT*>(att[i]);
      view[viewSz]                 = t.handle;
      fboLayout.RTVFormats[viewSz] = nativeFormat(frm[i]);
      ++viewSz;
      }
    else {
      auto& t = *reinterpret_cast<DxTextureWithRT*>(att[i]);
      fboLayout.DSVFormat = nativeFormat(frm[i]);
      ds = &t.handle;
      }
    }
  fboLayout.NumRenderTargets = viewSz;
  if(ds==nullptr)
    fboLayout.DSVFormat = DXGI_FORMAT_UNKNOWN;

  impl->OMSetRenderTargets(viewSz, view, FALSE, ds);
  state = RenderPass;

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

  viewSz = 0;
  for(size_t i=0; i<descSize; ++i) {
    auto& dx = desc[i];
    if(dx.load==AccessOp::Clear) {
      if(dx.attachment!=nullptr) {
        const float clearColor[] = { dx.clear.x, dx.clear.y, dx.clear.z, dx.clear.w };
        impl->ClearRenderTargetView(view[viewSz], clearColor, 0, nullptr);
        } else {
        impl->ClearDepthStencilView(*ds, D3D12_CLEAR_FLAG_DEPTH, dx.clear.x, 0, 0, nullptr);
        }
      }
    if(dx.attachment!=nullptr)
      ++viewSz;
    }
  }

void Tempest::Detail::DxCommandBuffer::endRendering() {
  // resState.flushLayout(*this);
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

void Tempest::Detail::DxCommandBuffer::setPipeline(Tempest::AbstractGraphicsApi::Pipeline& p) {
  DxPipeline& px = reinterpret_cast<DxPipeline&>(p);
  vboStride    = px.stride;
  ssboBarriers = px.ssboBarriers;

  impl->SetPipelineState(&px.instance(fboLayout));
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
  if(state!=Compute) {
    resState.flush(*this);
    state = Compute;
    }
  auto& px = reinterpret_cast<DxCompPipeline&>(p);
  impl->SetPipelineState(px.impl.get());
  impl->SetComputeRootSignature(px.sign.get());
  ssboBarriers = px.ssboBarriers;
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
  curUniforms = &ux;

  bool setH = false;
  for(size_t i=0;i<DxPipelineLay::MAX_BINDS;++i) {
    if(ux.val.heap[i]!=currentHeaps[i]) {
      setH = true;
      break;
      }
    }

  if(setH) {
    for(size_t i=0;i<DxPipelineLay::MAX_BINDS;++i)
      currentHeaps[i] = ux.val.heap[i];
    impl->SetDescriptorHeaps(ux.heapCnt, currentHeaps);
    }

  auto& lx = *ux.lay.handler;
  for(size_t i=0;i<lx.roots.size();++i) {
    auto& r   = lx.roots[i];
    auto desc = ux.val.gpu[r.heap];
    desc.ptr+=r.heapOffset;
    if(isCompute)
      impl->SetComputeRootDescriptorTable (UINT(i), desc); else
      impl->SetGraphicsRootDescriptorTable(UINT(i), desc);
    }
  }

void DxCommandBuffer::barrier(const AbstractGraphicsApi::BarrierDesc* desc, size_t cnt) {
  for(size_t i=0; i<cnt; ++i) {
    auto& b = desc[i];
    if(b.buffer!=nullptr)
      continue;

    ID3D12Resource* nativeImg = nullptr;
    if(b.texture!=nullptr) {
      DxTexture& t   = *reinterpret_cast<DxTexture*>(b.texture);
      nativeImg     = t.impl.get();
      } else {
      DxSwapchain& s = *reinterpret_cast<DxSwapchain*>(b.swapchain);
      nativeImg     = s.views[b.swId].get();
      }

    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.Transition.pResource   = nativeImg;
    barrier.Transition.StateBefore = Detail::nativeFormat(b.prev);
    barrier.Transition.StateAfter  = Detail::nativeFormat(b.next);
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

    if(barrier.Transition.StateBefore!=barrier.Transition.StateAfter)
      impl->ResourceBarrier(1,&barrier);

    if(!b.preserve && (b.next==ResourceLayout::ColorAttach || b.next==ResourceLayout::ColorAttach))
      impl->DiscardResource(nativeImg,nullptr);
    }

  for(size_t i=0; i<cnt; ++i) {
    auto& b = desc[i];
    if(b.buffer==nullptr)
      continue;
    DxBuffer& buf = *reinterpret_cast<DxBuffer*>(b.buffer);
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    barrier.Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    barrier.UAV.pResource          = buf.impl.get();

    impl->ResourceBarrier(1, &barrier);
    }
  }

void DxCommandBuffer::changeLayout(AbstractGraphicsApi::Texture& t,
                                   ResourceLayout prev, ResourceLayout next, uint32_t mipId) {
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

void DxCommandBuffer::copy(AbstractGraphicsApi::Buffer& dstBuf, ResourceLayout defLayout,
                           uint32_t width, uint32_t height, uint32_t mip,
                           AbstractGraphicsApi::Texture& srcTex, size_t offset) {
  resState.flush(*this);

  auto& dst = reinterpret_cast<DxBuffer&>(dstBuf);
  auto& src = reinterpret_cast<const DxTexture&>(srcTex);

  const UINT bpp       = src.bitCount()/8;
  const UINT pitchBase = UINT(width)*bpp;
  const UINT pitch     = ((pitchBase+D3D12_TEXTURE_DATA_PITCH_ALIGNMENT-1)/D3D12_TEXTURE_DATA_PITCH_ALIGNMENT)*D3D12_TEXTURE_DATA_PITCH_ALIGNMENT;

  if(pitch==pitchBase && (offset%D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT)==0) {
    changeLayout(srcTex,defLayout,ResourceLayout::TransferSrc,mip);
    copyRaw(dstBuf,width,height,mip,srcTex,offset);
    changeLayout(srcTex,ResourceLayout::TransferSrc,defLayout,mip);
    return;
    }

  std::unique_ptr<CopyBuf> dx(new CopyBuf(dev,dst,offset,src,width,height,mip));
  pushStage(dx.release());
  }

void DxCommandBuffer::draw(const AbstractGraphicsApi::Buffer& ivbo, size_t offset, size_t vertexCount, size_t firstInstance, size_t instanceCount) {
  if(T_UNLIKELY(ssboBarriers)) {
    curUniforms->ssboBarriers(resState);
    }
  const DxBuffer& vbo = reinterpret_cast<const DxBuffer&>(ivbo);
  D3D12_VERTEX_BUFFER_VIEW view;
  view.BufferLocation = vbo.impl.get()->GetGPUVirtualAddress();
  view.SizeInBytes    = vbo.sizeInBytes;
  view.StrideInBytes  = vboStride;
  impl->IASetVertexBuffers(0,1,&view);
  impl->DrawInstanced(UINT(vertexCount),UINT(instanceCount),UINT(offset),UINT(firstInstance));
  }

void DxCommandBuffer::drawIndexed(const AbstractGraphicsApi::Buffer& ivbo, const AbstractGraphicsApi::Buffer& iibo, Detail::IndexClass cls,
                                  size_t ioffset, size_t isize, size_t voffset, size_t firstInstance, size_t instanceCount) {
  if(T_UNLIKELY(ssboBarriers)) {
    curUniforms->ssboBarriers(resState);
    }
  static const DXGI_FORMAT type[]={
    DXGI_FORMAT_R16_UINT,
    DXGI_FORMAT_R32_UINT
    };
  const DxBuffer& vbo = reinterpret_cast<const DxBuffer&>(ivbo);
  const DxBuffer& ibo = reinterpret_cast<const DxBuffer&>(iibo);

  D3D12_VERTEX_BUFFER_VIEW view;
  view.BufferLocation = vbo.impl.get()->GetGPUVirtualAddress();
  view.SizeInBytes    = vbo.sizeInBytes;
  view.StrideInBytes  = vboStride;
  impl->IASetVertexBuffers(0,1,&view);

  D3D12_INDEX_BUFFER_VIEW iview;
  iview.BufferLocation = ibo.impl.get()->GetGPUVirtualAddress();
  iview.SizeInBytes    = ibo.sizeInBytes;
  iview.Format         = type[uint32_t(cls)];
  impl->IASetIndexBuffer(&iview);

  impl->DrawIndexedInstanced(UINT(isize),UINT(instanceCount),UINT(ioffset),INT(voffset),UINT(firstInstance));
  }

void DxCommandBuffer::dispatch(size_t x, size_t y, size_t z) {
  if(T_UNLIKELY(ssboBarriers)) {
    curUniforms->ssboBarriers(resState);
    resState.flush(*this);
    }
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

void DxCommandBuffer::clearStage() {
  while(stageResources!=nullptr) {
    auto s = stageResources;
    stageResources = stageResources->next;
    delete s;
    }
  }

void DxCommandBuffer::pushStage(DxCommandBuffer::Stage* cmd) {
  cmd->next = stageResources;
  stageResources = cmd;
  stageResources->exec(*this);
  }

void DxCommandBuffer::blitFS(AbstractGraphicsApi::Texture& srcTex, uint32_t srcW, uint32_t srcH, uint32_t srcMip,
                             AbstractGraphicsApi::Texture& dstTex, uint32_t dstW, uint32_t dstH, uint32_t dstMip) {
  std::unique_ptr<Blit> dx(new Blit(dev,
                                    reinterpret_cast<DxTexture&>(srcTex),srcW,srcH,srcMip,
                                    reinterpret_cast<DxTexture&>(dstTex),dstW,dstH,dstMip));
  pushStage(dx.release());
  }

void DxCommandBuffer::generateMipmap(AbstractGraphicsApi::Texture& img, ResourceLayout defLayout,
                                     uint32_t texWidth, uint32_t texHeight, uint32_t mipLevels) {
  if(mipLevels==1)
    return;

  resState.flush(*this);

  std::unique_ptr<MipMaps> dx(new MipMaps(dev,reinterpret_cast<DxTexture&>(img),defLayout,texWidth,texHeight,mipLevels));
  pushStage(dx.release());
  }

void DxCommandBuffer::copyRaw(AbstractGraphicsApi::Buffer& dstBuf, uint32_t width, uint32_t height, uint32_t mip,
                              const AbstractGraphicsApi::Texture& srcTex, size_t offset) {
  auto& dst = reinterpret_cast<DxBuffer&>(dstBuf);
  auto& src = reinterpret_cast<const DxTexture&>(srcTex);

  uint32_t wBlk = width;
  switch(src.format) {
    case DXGI_FORMAT_BC1_TYPELESS:
    case DXGI_FORMAT_BC1_UNORM:
    case DXGI_FORMAT_BC1_UNORM_SRGB:
    case DXGI_FORMAT_BC4_TYPELESS:
    case DXGI_FORMAT_BC4_UNORM:
    case DXGI_FORMAT_BC4_SNORM:
    case DXGI_FORMAT_BC2_TYPELESS:
    case DXGI_FORMAT_BC2_UNORM:
    case DXGI_FORMAT_BC2_UNORM_SRGB:
    case DXGI_FORMAT_BC3_TYPELESS:
    case DXGI_FORMAT_BC3_UNORM:
    case DXGI_FORMAT_BC3_UNORM_SRGB:
    case DXGI_FORMAT_BC5_TYPELESS:
    case DXGI_FORMAT_BC5_UNORM:
    case DXGI_FORMAT_BC5_SNORM:
    case DXGI_FORMAT_BC6H_TYPELESS:
    case DXGI_FORMAT_BC6H_UF16:
    case DXGI_FORMAT_BC6H_SF16:
    case DXGI_FORMAT_BC7_TYPELESS:
    case DXGI_FORMAT_BC7_UNORM:
    case DXGI_FORMAT_BC7_UNORM_SRGB:
      wBlk  = (wBlk +3)/4;
      // height = (height+3)/4;
      break;
    default:
      break;
    }

  const UINT bpb       = src.bytePerBlockCount();
  const UINT pitchBase = UINT(wBlk)*bpb;
  const UINT pitch     = ((pitchBase+D3D12_TEXTURE_DATA_PITCH_ALIGNMENT-1)/D3D12_TEXTURE_DATA_PITCH_ALIGNMENT)*D3D12_TEXTURE_DATA_PITCH_ALIGNMENT;

  D3D12_PLACED_SUBRESOURCE_FOOTPRINT foot = {};
  foot.Offset             = offset;
  foot.Footprint.Format   = src.format;
  foot.Footprint.Width    = UINT(width);
  foot.Footprint.Height   = UINT(height);
  foot.Footprint.Depth    = 1;
  foot.Footprint.RowPitch = pitch;

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


