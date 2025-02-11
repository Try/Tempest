#if defined(TEMPEST_BUILD_DIRECTX12)

#include "dxcommandbuffer.h"
#include "dxbuffer.h"
#include "dxtexture.h"
#include "dxdevice.h"
#include "dxswapchain.h"
#include "dxfbolayout.h"
#include "dxpipeline.h"
#include "dxaccelerationstructure.h"

// #include <pix3.h>
// #include <WinPixEventRuntime/pix3.h>

#include "guid.h"

using namespace Tempest;
using namespace Tempest::Detail;

static void beginEvent(ID3D12GraphicsCommandList& cmd, uint32_t meta, const wchar_t* buf) {
  // NOTE: pix is too much trouble to integrate
  cmd.BeginEvent(meta, buf, UINT((std::wcslen(buf)+1)*sizeof(wchar_t)));
  }

static void endEvent(ID3D12GraphicsCommandList& cmd) {
  cmd.EndEvent();
  }

static D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE mkLoadOp(const AccessOp op) {
  switch(op) {
    case AccessOp::Discard:  return D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_DISCARD;
    case AccessOp::Preserve: return D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE;
    case AccessOp::Clear:    return D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR;
    case AccessOp::Readonly: return D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_PRESERVE;
    }
  return D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_NO_ACCESS;
  }

static D3D12_RENDER_PASS_ENDING_ACCESS_TYPE mkStoreOp(const AccessOp op) {
  switch(op) {
    case AccessOp::Discard:  return D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_DISCARD;
    case AccessOp::Preserve: return D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE;
    case AccessOp::Clear:    return D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE;
    case AccessOp::Readonly: return D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_PRESERVE;
    }
  return D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_NO_ACCESS;
  }

static void toStage(DxDevice& dev, D3D12_BARRIER_SYNC& stage, D3D12_BARRIER_ACCESS& access, ResourceAccess rs, bool isSrc, bool isTex) {
  uint32_t ret = 0;
  uint32_t acc = 0;
  if((rs&ResourceAccess::TransferSrc)==ResourceAccess::TransferSrc) {
    ret |= D3D12_BARRIER_SYNC_COPY;
    acc |= D3D12_BARRIER_ACCESS_COPY_SOURCE;
    }
  if((rs&ResourceAccess::TransferDst)==ResourceAccess::TransferDst) {
    ret |= D3D12_BARRIER_SYNC_COPY;
    acc |= D3D12_BARRIER_ACCESS_COPY_DEST;
    }
  if((rs&ResourceAccess::TransferHost)==ResourceAccess::TransferHost) {
    //ret |= VK_PIPELINE_STAGE_HOST_BIT;
    //acc |= VK_ACCESS_HOST_READ_BIT;
    }

  if((rs&ResourceAccess::Present)==ResourceAccess::Present) {
    ret |= D3D12_BARRIER_SYNC_NONE;
    acc |= D3D12_BARRIER_ACCESS_COMMON;
    }

  if((rs&ResourceAccess::Sampler)==ResourceAccess::Sampler) {
    ret |= D3D12_BARRIER_SYNC_ALL;
    acc |= D3D12_BARRIER_ACCESS_SHADER_RESOURCE;
    }
  if((rs&ResourceAccess::ColorAttach)==ResourceAccess::ColorAttach) {
    ret |= D3D12_BARRIER_SYNC_RENDER_TARGET;
    acc |= D3D12_BARRIER_ACCESS_RENDER_TARGET;
    }
  if((rs&ResourceAccess::DepthAttach)==ResourceAccess::DepthAttach) {
    ret |= D3D12_BARRIER_SYNC_DEPTH_STENCIL;
    acc |= D3D12_BARRIER_ACCESS_DEPTH_STENCIL_WRITE;
    }
  if((rs&ResourceAccess::DepthReadOnly)==ResourceAccess::DepthReadOnly) {
    ret |= D3D12_BARRIER_SYNC_ALL;
    acc |= D3D12_BARRIER_ACCESS_DEPTH_STENCIL_READ;
    }

  if((rs&ResourceAccess::Index)==ResourceAccess::Index) {
    // ret |= D3D12_BARRIER_SYNC_INPUT_ASSEMBLER;
    ret |= D3D12_BARRIER_SYNC_INDEX_INPUT;
    acc |= D3D12_BARRIER_ACCESS_INDEX_BUFFER;
    }
  if((rs&ResourceAccess::Vertex)==ResourceAccess::Vertex) {
    // ret |= D3D12_BARRIER_SYNC_INPUT_ASSEMBLER;
    ret |= D3D12_BARRIER_SYNC_INDEX_INPUT;
    acc |= D3D12_BARRIER_ACCESS_VERTEX_BUFFER;
    }
  if((rs&ResourceAccess::Uniform)==ResourceAccess::Uniform) {
    ret |= D3D12_BARRIER_SYNC_ALL;
    acc |= D3D12_BARRIER_ACCESS_CONSTANT_BUFFER;
    }
  if((rs&ResourceAccess::Indirect)==ResourceAccess::Indirect) {
    ret |= D3D12_BARRIER_SYNC_PREDICATION;
    acc |= D3D12_BARRIER_ACCESS_PREDICATION;
    }

  if(dev.props.raytracing.rayQuery) {
    if((rs&ResourceAccess::RtAsRead)==ResourceAccess::RtAsRead) {
      ret |= D3D12_BARRIER_SYNC_ALL;
      acc |= D3D12_BARRIER_ACCESS_RAYTRACING_ACCELERATION_STRUCTURE_READ;
      }
    if((rs&ResourceAccess::RtAsWrite)==ResourceAccess::RtAsWrite) {
      ret |= D3D12_BARRIER_SYNC_BUILD_RAYTRACING_ACCELERATION_STRUCTURE;
      acc |= D3D12_BARRIER_ACCESS_RAYTRACING_ACCELERATION_STRUCTURE_WRITE;
      }
    }

  // memory barriers
  if((rs&ResourceAccess::UavReadGr)==ResourceAccess::UavReadGr) {
    ret |= D3D12_BARRIER_SYNC_DRAW;
    acc |= D3D12_BARRIER_ACCESS_UNORDERED_ACCESS | D3D12_BARRIER_ACCESS_CONSTANT_BUFFER;
    }
  if((rs&ResourceAccess::UavWriteGr)==ResourceAccess::UavWriteGr) {
    ret |= D3D12_BARRIER_SYNC_DRAW;
    acc |= D3D12_BARRIER_ACCESS_UNORDERED_ACCESS;
    }

  if((rs&ResourceAccess::UavReadComp)==ResourceAccess::UavReadComp) {
    ret |= D3D12_BARRIER_SYNC_COMPUTE_SHADING;
    acc |= D3D12_BARRIER_ACCESS_UNORDERED_ACCESS | D3D12_BARRIER_ACCESS_CONSTANT_BUFFER;
    }
  if((rs&ResourceAccess::UavWriteComp)==ResourceAccess::UavWriteComp) {
    ret |= D3D12_BARRIER_SYNC_COMPUTE_SHADING;
    acc |= D3D12_BARRIER_ACCESS_UNORDERED_ACCESS;
    }

  const D3D12_BARRIER_ACCESS maskTex = D3D12_BARRIER_ACCESS_RENDER_TARGET | D3D12_BARRIER_ACCESS_UNORDERED_ACCESS |
                                       D3D12_BARRIER_ACCESS_DEPTH_STENCIL_WRITE | D3D12_BARRIER_ACCESS_DEPTH_STENCIL_READ |
                                       D3D12_BARRIER_ACCESS_SHADER_RESOURCE;

  if(isTex) {
    acc &= maskTex;
    }

  if(isSrc && ret==0) {
    // wait for nothing: asset uploading case
    ret = D3D12_BARRIER_SYNC_NONE;
    acc = D3D12_BARRIER_ACCESS_NO_ACCESS;
    }

  if(ret==D3D12_BARRIER_SYNC_NONE) {
    acc = D3D12_BARRIER_ACCESS_NO_ACCESS;
    }

  stage  = D3D12_BARRIER_SYNC  (ret);
  access = D3D12_BARRIER_ACCESS(acc);
  }

static D3D12_BARRIER_LAYOUT toLayout(ResourceAccess rs) {
  if(rs==ResourceAccess::None)
    return D3D12_BARRIER_LAYOUT_UNDEFINED;

  if((rs&ResourceAccess::TransferSrc)==ResourceAccess::TransferSrc)
    return D3D12_BARRIER_LAYOUT_COPY_SOURCE;
  if((rs&ResourceAccess::TransferDst)==ResourceAccess::TransferDst)
    return D3D12_BARRIER_LAYOUT_COPY_DEST;

  if((rs&ResourceAccess::Present)==ResourceAccess::Present)
    return D3D12_BARRIER_LAYOUT_PRESENT;

  if((rs&ResourceAccess::Sampler)==ResourceAccess::Sampler)
    return D3D12_BARRIER_LAYOUT_SHADER_RESOURCE;
  if((rs&ResourceAccess::ColorAttach)==ResourceAccess::ColorAttach)
    return D3D12_BARRIER_LAYOUT_RENDER_TARGET;
  if((rs&ResourceAccess::DepthReadOnly)==ResourceAccess::DepthReadOnly)
    return D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_READ;
  if((rs&ResourceAccess::DepthAttach)==ResourceAccess::DepthAttach)
    return D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_WRITE;

  return D3D12_BARRIER_LAYOUT_UNORDERED_ACCESS;
  }

static D3D12_RESOURCE_STATES nativeFormat(ResourceAccess rs) {
  uint32_t st = 0;

  if(rs==ResourceAccess::None)
    return D3D12_RESOURCE_STATE_COMMON;

  if((rs&ResourceAccess::TransferSrc)==ResourceAccess::TransferSrc)
    st |= D3D12_RESOURCE_STATE_COPY_SOURCE;
  if((rs&ResourceAccess::TransferDst)==ResourceAccess::TransferDst)
    st |= D3D12_RESOURCE_STATE_COPY_DEST;
  if((rs&ResourceAccess::TransferHost)==ResourceAccess::TransferHost)
    st |= D3D12_RESOURCE_STATE_COPY_SOURCE;

  if((rs&ResourceAccess::Present)==ResourceAccess::Present)
    st |= D3D12_RESOURCE_STATE_COMMON;

  if((rs&ResourceAccess::Sampler)==ResourceAccess::Sampler)
    st |= D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE|D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
  if((rs&ResourceAccess::ColorAttach)==ResourceAccess::ColorAttach)
    st |= D3D12_RESOURCE_STATE_RENDER_TARGET;
  if((rs&ResourceAccess::DepthAttach)==ResourceAccess::DepthAttach)
    st |= D3D12_RESOURCE_STATE_DEPTH_WRITE;
  if((rs&ResourceAccess::DepthReadOnly)==ResourceAccess::DepthReadOnly)
    st |= D3D12_RESOURCE_STATE_DEPTH_READ;

  if((rs&ResourceAccess::Vertex)==ResourceAccess::Vertex)
    st |= D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
  if((rs&ResourceAccess::Index)==ResourceAccess::Index)
    st |= D3D12_RESOURCE_STATE_INDEX_BUFFER;
  if((rs&ResourceAccess::Uniform)==ResourceAccess::Uniform)
    st |= D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
  if((rs&ResourceAccess::Indirect)==ResourceAccess::Indirect)
    st |= D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT;

  if(true) {
    if((rs&ResourceAccess::RtAsRead)==ResourceAccess::RtAsRead) {
      st |= D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
      }
    if((rs&ResourceAccess::RtAsWrite)==ResourceAccess::RtAsWrite) {
      st |= D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
      }
    }

  // memory barriers
  if((rs&ResourceAccess::UavReadGr)==ResourceAccess::UavReadGr) {
    st |= D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    }
  if((rs&ResourceAccess::UavWriteGr)==ResourceAccess::UavWriteGr) {
    st |= D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    }

  if((rs&ResourceAccess::UavReadComp)==ResourceAccess::UavReadComp) {
    st |= D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    }
  if((rs&ResourceAccess::UavWriteComp)==ResourceAccess::UavWriteComp) {
    st |= D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    }

  return D3D12_RESOURCE_STATES(st);
  }

static ID3D12Resource* toDxResource(const AbstractGraphicsApi::BarrierDesc& b) {
  if(b.texture!=nullptr) {
    DxTexture& t = *reinterpret_cast<DxTexture*>(b.texture);
    return t.impl.get();
    }

  if(b.buffer!=nullptr) {
    auto& buf = *reinterpret_cast<const DxBuffer*>(b.buffer);
    return buf.impl.get();
    }

  DxSwapchain& s = *reinterpret_cast<DxSwapchain*>(b.swapchain);
  return s.views[b.swId].get();
  }


struct DxCommandBuffer::MipMaps : Stage {
  MipMaps(DxDevice& dev, DxTexture& image, uint32_t texW, uint32_t texH, uint32_t mipLevels)
    :img(image),texW(texW),texH(texH),mipLevels(mipLevels) {
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
/*
    desc.emplace_back(*dev.blitLayout.handler);
    DxDescriptorArray& ubo = this->desc.back();
    ubo.set(0,&img,Sampler::bilinear(),srcMip);
    cmd.implSetUniforms(ubo,false);

    impl.DrawInstanced(6,1,0,0);
    */
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

    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle  = rtvHeap->GetCPUDescriptorHandleForHeapStart();
    auto                        rtvHeapInc = dev.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

    auto& resState = cmd.resState;
    resState.setLayout(img,ResourceAccess::ColorAttach);
    resState.flush(cmd);
    for(uint32_t i=1; i<mipLevels; ++i) {
      const int mw = (w==1 ? 1 : w/2);
      const int mh = (h==1 ? 1 : h/2);

      cmd.barrier(img,ResourceAccess::ColorAttach,ResourceAccess::Sampler,i-1);
      impl.OMSetRenderTargets(1, &rtvHandle, TRUE, nullptr);
      blit(cmd,i-1,mw,mh);

      w             = mw;
      h             = mh;
      rtvHandle.ptr+= rtvHeapInc;
      }
    cmd.barrier(img, ResourceAccess::ColorAttach, ResourceAccess::Sampler, mipLevels-1);
    }

  DxTexture&                     img;
  uint32_t                       texW;
  uint32_t                       texH;
  uint32_t                       mipLevels;

  ComPtr<ID3D12DescriptorHeap>   rtvHeap;
  std::vector<DxDescriptorArray> desc;
  };

struct DxCommandBuffer::FillUAV : Stage {
  using Allocation = DxDescriptorAllocator::Allocation;

  FillUAV(DxDevice& dev, DxTexture& image, uint32_t val)
    :dst(image), val(val) {
    auto& allocator = dev.descAlloc;
    gpu = allocator.alloc    (dst.mipCnt, false);
    cpu = allocator.allocHost(dst.mipCnt);

    hSize = dev.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    for(uint32_t i=0; i<dst.mipCnt; ++i) {
      D3D12_UNORDERED_ACCESS_VIEW_DESC desc = {};
      uint32_t mipLevel = 0;
      desc.Format = dst.format;
      if(dst.is3D) {
        desc.ViewDimension      = D3D12_UAV_DIMENSION_TEXTURE3D;
        desc.Texture3D.MipSlice = mipLevel;
        desc.Texture3D.WSize    = dst.sliceCnt;
        } else {
        desc.ViewDimension      = D3D12_UAV_DIMENSION_TEXTURE2D;
        desc.Texture2D.MipSlice = mipLevel;
        }

      auto g = allocator.handle(gpu);
      auto c = allocator.handle(cpu);
      g.ptr += hSize*i;
      c.ptr += hSize*i;
      dev.device->CreateUnorderedAccessView(dst.impl.get(),nullptr,&desc,g);
      dev.device->CreateUnorderedAccessView(dst.impl.get(),nullptr,&desc,c);
      }
    }

  void exec(DxCommandBuffer& cmd) override {
    auto& allocator = cmd.dev.descAlloc;

    cmd.resState.onTranferUsage(NonUniqResId::I_None, dst.nonUniqId, false);
    cmd.resState.flush(cmd);

    cmd.curHeaps.heaps[0] = allocator.heapof(gpu);
    cmd.curHeaps.heaps[1] = nullptr;
    cmd.impl->SetDescriptorHeaps(1, cmd.curHeaps.heaps);

    UINT val4[] = {val,val,val,val};
    for(uint32_t i=0; i<dst.mipCnt; ++i) {
      auto g = allocator.gpuHandle(gpu);
      auto c = allocator.handle(cpu);
      g.ptr += hSize*i;
      c.ptr += hSize*i;
      cmd.impl->ClearUnorderedAccessViewUint(g, c, dst.impl.get(), val4, 0, nullptr);
      }
    }

  DxTexture&                     dst;
  uint32_t                       val = 0;
  UINT                           hSize = 0;
  Allocation                     cpu, gpu;
  };

DxCommandBuffer::DxCommandBuffer(DxDevice& d)
  : dev(d), pushDescriptors(d) {
  dxAssert(d.device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                            uuid<ID3D12CommandAllocator>(),
                                            reinterpret_cast<void**>(&pool)));
  }

DxCommandBuffer::~DxCommandBuffer() {
  clearStage();
  auto node = chunks.begin();
  for(size_t i=0; i<chunks.size(); ++i) {
    auto cmd = node->val[i%chunks.chunkSize].impl;
    dxAssert(cmd->Release());
    if(i+1==chunks.chunkSize)
      node = node->next;
    }
  }

void DxCommandBuffer::begin(bool transfer) {
  reset();
  state = Idle;
  if(transfer)
    resState.clearReaders();

  if(impl.get()==nullptr) {
    newChunk();
    }
  }

void DxCommandBuffer::begin() {
  begin(false);
  }

void DxCommandBuffer::end() {
  if(isDbgRegion) {
    endEvent(*impl);
    isDbgRegion = false;
    }
  resState.finalize(*this);
  state     = NoRecording;
  resetDone = false;

  pushChunk();
  }

void DxCommandBuffer::reset() {
  if(resetDone)
    return;
  clearStage();

  dxAssert(pool->Reset());
  SmallArray<ID3D12CommandList*,MaxCmdChunks> flat(chunks.size());
  auto node = chunks.begin();
  if(chunks.size()>0) {
    impl = ComPtr<ID3D12GraphicsCommandList6>(node->val[0].impl);
    dxAssert(impl->Reset(pool.get(),nullptr));
    }
  for(size_t i=1; i<chunks.size(); ++i) {
    auto cmd = node->val[i%chunks.chunkSize].impl;
    cmd->Release();
    // dxAssert(cmd->Reset(pool.get(),nullptr));
    if(i+1==chunks.chunkSize)
      node = node->next;
    }
  chunks.clear();
  pushDescriptors.reset();
  resetDone   = true;
  }

bool DxCommandBuffer::isRecording() const {
  return state!=NoRecording;
  }

void DxCommandBuffer::beginRendering(const AttachmentDesc* desc, size_t descSize, uint32_t w, uint32_t h,
                                     const TextureFormat* frm, AbstractGraphicsApi::Texture** att,
                                     AbstractGraphicsApi::Swapchain** sw, const uint32_t* imgId) {
  resState.joinWriters(PipelineStage::S_Indirect);
  resState.joinWriters(PipelineStage::S_Graphics);
  resState.setRenderpass(*this,desc,descSize,frm,att,sw,imgId);

  bindings.read  = NonUniqResId::I_None;
  bindings.write = NonUniqResId::I_None;

  if(state!=Idle) {
    newChunk();
    }

  D3D12_RENDER_PASS_RENDER_TARGET_DESC view[MaxFramebufferAttachments] = {};
  UINT                                 viewSz = 0;
  D3D12_RENDER_PASS_DEPTH_STENCIL_DESC zdesc  = {};
  D3D12_RENDER_PASS_FLAGS              flags  = D3D12_RENDER_PASS_FLAG_ALLOW_UAV_WRITES; //D3D12_RENDER_PASS_FLAG_NONE;
  zdesc.DepthBeginningAccess.Type   = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_NO_ACCESS;
  zdesc.DepthEndingAccess.Type      = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_NO_ACCESS;
  zdesc.StencilBeginningAccess.Type = D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_NO_ACCESS;
  zdesc.StencilEndingAccess.Type    = D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_NO_ACCESS;
  for(size_t i=0; i<descSize; ++i) {
    auto& dx    = desc[i];
    auto& rdesc = view[viewSz];
    if(sw[i]!=nullptr) {
      auto& t                      = *reinterpret_cast<DxSwapchain*>(sw[i]);
      rdesc.cpuDescriptor          = t.handles[imgId[i]];
      fboLayout.RTVFormats[viewSz] = t.format();
      ++viewSz;
      }
    else if(desc[i].attachment!=nullptr) {
      auto& t                      = *reinterpret_cast<DxTextureWithRT*>(att[i]);
      rdesc.cpuDescriptor          = t.handle;
      fboLayout.RTVFormats[viewSz] = nativeFormat(frm[i]);
      ++viewSz;
      }
    else {
      auto& t = *reinterpret_cast<DxTextureWithRT*>(att[i]);
      fboLayout.DSVFormat = nativeFormat(frm[i]);
      zdesc.cpuDescriptor = (dx.load==AccessOp::Readonly ? t.handleR : t.handle);
      if(dx.load==AccessOp::Readonly) {
        const uint32_t D3D12_RENDER_PASS_FLAG_BIND_READ_ONLY_DEPTH = 0x8; // headers?
        flags = D3D12_RENDER_PASS_FLAGS(flags | D3D12_RENDER_PASS_FLAG_BIND_READ_ONLY_DEPTH);
        }
      }

    if(desc[i].zbuffer!=nullptr) {
      zdesc.DepthBeginningAccess.Type                                = mkLoadOp(dx.load);
      zdesc.DepthEndingAccess.Type                                   = mkStoreOp(dx.store);
      zdesc.DepthBeginningAccess.Clear.ClearValue.Format             = DXGI_FORMAT_D32_FLOAT;
      zdesc.DepthBeginningAccess.Clear.ClearValue.DepthStencil.Depth = dx.clear.x;
      } else {
      rdesc.BeginningAccess.Type                      = mkLoadOp(dx.load);
      rdesc.EndingAccess.Type                         = mkStoreOp(dx.store);
      rdesc.BeginningAccess.Clear.ClearValue.Format   = DXGI_FORMAT_R32G32B32A32_FLOAT;
      rdesc.BeginningAccess.Clear.ClearValue.Color[0] = dx.clear.x;
      rdesc.BeginningAccess.Clear.ClearValue.Color[1] = dx.clear.y;
      rdesc.BeginningAccess.Clear.ClearValue.Color[2] = dx.clear.z;
      rdesc.BeginningAccess.Clear.ClearValue.Color[3] = dx.clear.w;
      }
    }

  fboLayout.NumRenderTargets = viewSz;
  if(zdesc.DepthBeginningAccess.Type==D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_NO_ACCESS)
    fboLayout.DSVFormat = DXGI_FORMAT_UNKNOWN;

  if(zdesc.DepthBeginningAccess.Type==D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_NO_ACCESS) {
    impl->BeginRenderPass(viewSz, view, nullptr, flags);
    } else {
    impl->BeginRenderPass(viewSz, view, &zdesc,  flags);
    }
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
  }

void DxCommandBuffer::endRendering() {
  resState.onUavUsage(bindings.read, bindings.write, PipelineStage::S_Graphics);

  impl->EndRenderPass();
  restoreIndirect();
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

void DxCommandBuffer::setScissor(const Rect& r) {
  D3D12_RECT sr={};
  sr.left   = r.x;
  sr.top    = r.y;
  sr.right  = LONG(r.x+r.w);
  sr.bottom = LONG(r.y+r.h);
  impl->RSSetScissorRects(1, &sr);
  }

void DxCommandBuffer::setDebugMarker(std::string_view tag) {
  if(isDbgRegion) {
    endEvent(*impl);
    isDbgRegion = false;
    }

  if(!tag.empty()) {
    wchar_t buf[128] = {};
    for(size_t i=0; i<127 && i<tag.size(); ++i)
      buf[i] = tag[i];
    beginEvent(*impl, 0x0, buf);
    isDbgRegion = true;
    }
  }

void DxCommandBuffer::setComputePipeline(AbstractGraphicsApi::CompPipeline& p) {
  auto& px = reinterpret_cast<DxCompPipeline&>(p);

  bindings.durty  = true;
  pushData.durty  = true;
  pushConstantId  = px.pushConstantId;

  curCompPipeline = &px;

  state = Compute;
  impl->SetPipelineState(px.impl.get());
  impl->SetComputeRootSignature(px.sign.get());
  }

void DxCommandBuffer::setPushData(const void* data, size_t size) {
  pushData.size = uint8_t(size);
  std::memcpy(pushData.data, data, size);

  pushData.durty = true;
  }

void DxCommandBuffer::setBinding(size_t id, AbstractGraphicsApi::Texture* tex, const Sampler& smp, uint32_t mipLevel) {
  bindings.data  [id] = tex;
  bindings.smp   [id] = smp;
  bindings.offset[id] = mipLevel;
  bindings.durty      = true;
  bindings.array      = bindings.array & ~(1u << id);
  }

void DxCommandBuffer::setBinding(size_t id, AbstractGraphicsApi::Buffer* buf, size_t offset) {
  bindings.data  [id] = buf;
  bindings.offset[id] = uint32_t(offset);
  bindings.durty      = true;
  bindings.array      = bindings.array & ~(1u << id);
  }

void DxCommandBuffer::setBinding(size_t id, AbstractGraphicsApi::DescArray* arr) {
  bindings.data[id] = arr;
  bindings.durty    = true;
  bindings.array    = bindings.array | (1u << id);
  }

void DxCommandBuffer::setBinding(size_t id, AbstractGraphicsApi::AccelerationStructure* tlas) {
  bindings.data[id] = tlas;
  bindings.durty    = true;
  bindings.array    = bindings.array & ~(1u << id);
  }

void DxCommandBuffer::setBinding(size_t id, const Sampler& smp) {
  bindings.smp[id] = smp;
  bindings.durty   = true;
  bindings.array   = bindings.array & ~(1u << id);
  }

void DxCommandBuffer::dispatch(size_t x, size_t y, size_t z) {
  implSetUniforms(PipelineStage::S_Compute);
  implSetPushData(PipelineStage::S_Compute);
  resState.onUavUsage(bindings.read, bindings.write, PipelineStage::S_Compute);
  resState.flush(*this);
  impl->Dispatch(UINT(x),UINT(y),UINT(z));
  }

void DxCommandBuffer::dispatchIndirect(const AbstractGraphicsApi::Buffer& indirect, size_t offset) {
  const DxBuffer& ind  = reinterpret_cast<const DxBuffer&>(indirect);
  auto&           sign = dev.dispatchIndirectSgn.get();

  implSetUniforms(PipelineStage::S_Compute);
  implSetPushData(PipelineStage::S_Compute);
  resState.onUavUsage(bindings.read, bindings.write, PipelineStage::S_Compute);
  // block future writers
  resState.onUavUsage(ind.nonUniqId, NonUniqResId::I_None, PipelineStage::S_Indirect);
  if(!dev.props.enhancedBarriers)
    resState.setLayout(indirect, ResourceAccess::Indirect);
  resState.flush(*this);

  impl->ExecuteIndirect(sign, 1, ind.impl.get(), UINT64(offset), nullptr, 0);
  }

void DxCommandBuffer::setPipeline(Tempest::AbstractGraphicsApi::Pipeline& p) {
  DxPipeline& px     = reinterpret_cast<DxPipeline&>(p);

  bindings.durty     = true;
  pushData.durty     = true;
  pushBaseInstanceId = px.pushBaseInstanceId;
  pushConstantId     = px.pushConstantId;
  curDrawPipeline    = &px;

  impl->SetPipelineState(&px.instance(fboLayout));
  impl->SetGraphicsRootSignature(px.sign.get());
  impl->IASetPrimitiveTopology(px.topology);
  }

void DxCommandBuffer::handleSync(const DxPipelineLay::LayoutDesc& lay, const DxPipelineLay::SyncDesc& sync, PipelineStage st) {
  if(st!=PipelineStage::S_Graphics) {
    bindings.read  = NonUniqResId::I_None;
    bindings.write = NonUniqResId::I_None;
    }

  for(size_t i=0; i<MaxBindings; ++i) {
    NonUniqResId nonUniqId = NonUniqResId::I_None;
    auto         data      = bindings.data[i];
    if(data==nullptr)
      continue;
    switch(lay.bindings[i]) {
      case ShaderReflection::Texture:
      case ShaderReflection::Image:
      case ShaderReflection::ImgR:
      case ShaderReflection::ImgRW: {
        nonUniqId = reinterpret_cast<DxTexture*>(data)->nonUniqId;
        break;
        }
      case ShaderReflection::Ubo:
      case ShaderReflection::SsboR:
      case ShaderReflection::SsboRW: {
        nonUniqId = reinterpret_cast<DxBuffer*>(data)->nonUniqId;
        break;
        }
      case ShaderReflection::Tlas: {
        //NOTE: Tlas tracking is not really implemented
        nonUniqId = reinterpret_cast<DxAccelerationStructure*>(data)->impl.nonUniqId;
        break;
        }
      case ShaderReflection::Sampler:
      case ShaderReflection::Push:
      case ShaderReflection::Count:
        break;
      }

    if(sync.read & (1u<<i))
      bindings.read |= nonUniqId;
    if(sync.write & (1u<<i))
      bindings.write |= nonUniqId;
    }
  }

void DxCommandBuffer::implSetUniforms(const PipelineStage st) {
  if(!bindings.durty)
    return;
  bindings.durty = false;

  const DxPipelineLay* lay = nullptr;
  switch(st) {
    case PipelineStage::S_Graphics:
      lay = &curDrawPipeline->layout;
      break;
    case PipelineStage::S_Compute:
      lay = &curCompPipeline->layout;
      break;
    default:
      break;
    }

  handleSync(lay->layout, lay->sync, st);

  const auto dset = pushDescriptors.push(lay->pb, lay->layout, bindings);
  pushDescriptors.setHeap(*impl, curHeaps);
  pushDescriptors.setRootTable(*impl, dset, lay->layout, bindings, st);
  }

void DxCommandBuffer::implSetPushData(const PipelineStage st) {
  if(!pushData.durty)
    return;
  pushData.durty = false;

  const DxPipelineLay* lay  = nullptr;
  switch(st) {
    case PipelineStage::S_Graphics:
      lay = &curDrawPipeline->layout;
      break;
    case PipelineStage::S_Compute:
      lay = &curCompPipeline->layout;
      break;
    default:
      break;
    }

  if(lay->pb.size==0)
    return;

  assert(lay->pb.size<=pushData.size);

  const uint32_t size = uint32_t(lay->pb.size);
  if(st==S_Graphics)
    impl->SetGraphicsRoot32BitConstants(pushConstantId, UINT(size/4), pushData.data, 0); else
    impl->SetComputeRoot32BitConstants(pushConstantId, UINT(size/4), pushData.data, 0);
  }

void DxCommandBuffer::restoreIndirect() {
  /*
  for(auto i:indirectCmd) {
    barrier(*i, ResourceAccess::Indirect, ResourceAccess::None); // to common state
    }
  indirectCmd.clear();
  */
  }

void DxCommandBuffer::enhancedBarrier(const AbstractGraphicsApi::BarrierDesc* desc, size_t cnt) {
  D3D12_BUFFER_BARRIER      bufBarrier[MaxBarriers] = {};
  uint32_t                  bufCount = 0;
  D3D12_TEXTURE_BARRIER     imgBarrier[MaxBarriers] = {};
  uint32_t                  imgCount = 0;
  D3D12_GLOBAL_BARRIER      memBarrier = {};
  uint32_t                  memCount  = 0;

  for(size_t i=0; i<cnt; ++i) {
    auto& b  = desc[i];

    if(b.buffer==nullptr && b.texture==nullptr && b.swapchain==nullptr) {
      D3D12_BARRIER_SYNC   srcStageMask  = D3D12_BARRIER_SYNC_NONE;
      D3D12_BARRIER_ACCESS srcAccessMask = D3D12_BARRIER_ACCESS_COMMON;
      D3D12_BARRIER_SYNC   dstStageMask  = D3D12_BARRIER_SYNC_NONE;
      D3D12_BARRIER_ACCESS dstAccessMask = D3D12_BARRIER_ACCESS_COMMON;
      toStage(dev, srcStageMask, srcAccessMask, b.prev, true,  false);
      toStage(dev, dstStageMask, dstAccessMask, b.next, false, false);

      memBarrier.SyncBefore   |= srcStageMask;
      memBarrier.AccessBefore |= srcAccessMask;
      memBarrier.SyncAfter    |= dstStageMask;
      memBarrier.AccessAfter  |= dstAccessMask;
      memCount = 1;
      }
    else if(b.buffer!=nullptr) {
      auto& bx = bufBarrier[bufCount];
      ++bufCount;

      toStage(dev, bx.SyncBefore, bx.AccessBefore, b.prev, true,  false);
      toStage(dev, bx.SyncAfter,  bx.AccessAfter,  b.next, false, false);
      bx.pResource = toDxResource(b);
      bx.Offset    = 0;
      bx.Size      = UINT64_MAX;
      }
    else {
      auto& bx = imgBarrier[imgCount];
      ++imgCount;

      toStage(dev, bx.SyncBefore, bx.AccessBefore, b.prev, true,  true);
      toStage(dev, bx.SyncAfter,  bx.AccessAfter,  b.next, false, true);

      bx.LayoutBefore = toLayout(b.prev);
      bx.LayoutAfter  = toLayout(b.next);
      bx.pResource    = toDxResource(b);
      bx.Subresources.IndexOrFirstMipLevel = (b.mip==uint32_t(-1) ?  D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES : b.mip);
      bx.Flags        = D3D12_TEXTURE_BARRIER_FLAG_NONE;
      //finalizeImageBarrier(bx,b);
      }
    }

  for(size_t i=0; i<imgCount; ++i) {
    auto& bx = imgBarrier[i];
    if(bx.LayoutBefore!=D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_READ && bx.LayoutAfter!=D3D12_BARRIER_LAYOUT_DEPTH_STENCIL_READ)
      continue;
    if((bx.AccessBefore&D3D12_BARRIER_ACCESS_DEPTH_STENCIL_WRITE)!=0)
      Log::e("");
    if((bx.AccessAfter&D3D12_BARRIER_ACCESS_DEPTH_STENCIL_WRITE)!=0)
      Log::e("");
    }

  D3D12_BARRIER_GROUP gBuffer = {D3D12_BARRIER_TYPE_BUFFER, bufCount};
  gBuffer.pBufferBarriers = bufBarrier;

  D3D12_BARRIER_GROUP gTexture = {D3D12_BARRIER_TYPE_TEXTURE, imgCount};
  gTexture.pTextureBarriers = imgBarrier;

  D3D12_BARRIER_GROUP gGlobal = {D3D12_BARRIER_TYPE_GLOBAL, memCount};
  gGlobal.pGlobalBarriers = &memBarrier;

  D3D12_BARRIER_GROUP barrier[3]; uint32_t num = 0;
  if(gBuffer.NumBarriers>0) {
    barrier[num] = gBuffer;
    ++num;
    }
  if(gTexture.NumBarriers>0) {
    barrier[num] = gTexture;
    ++num;
    }
  if(gGlobal.NumBarriers>0) {
    barrier[num] = gGlobal;
    ++num;
    }

  ComPtr<ID3D12GraphicsCommandList7> cmd7;
  impl->QueryInterface(uuid<ID3D12GraphicsCommandList7>(), reinterpret_cast<void**>(&cmd7));
  cmd7->Barrier(num, barrier);
  }

void DxCommandBuffer::barrier(const AbstractGraphicsApi::BarrierDesc* desc, size_t cnt) {
  if(dev.props.enhancedBarriers) {
    enhancedBarrier(desc, cnt);
    return;
    }

  D3D12_RESOURCE_BARRIER rb[MaxBarriers];
  uint32_t               rbCount = 0;
  for(size_t i=0; i<cnt; ++i) {
    auto& b = desc[i];
    D3D12_RESOURCE_BARRIER& barrier = rb[rbCount];
    if(b.buffer==nullptr && b.texture==nullptr && b.swapchain==nullptr) {
      /* HACK:
       * 1: use UAV barrier to sync random access from compute-like shaders (and graphics side effects)
       * 2: use aliasing barrier to force all-to-all execution dependency with cache-flush (out of spec behaviour)
       * 3: ideally we need Enhanced-Barrier. This code is used only as fallback
       */
      const ResourceAccess mask = ResourceAccess(~uint32_t(ResourceAccess::UavWriteGr | ResourceAccess::UavWriteComp));
      if((b.prev & mask)==ResourceAccess::None && (b.next & mask)==ResourceAccess::None) {
        barrier.Type                     = D3D12_RESOURCE_BARRIER_TYPE_UAV;
        barrier.Flags                    = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrier.UAV.pResource            = nullptr;
        } else {
        barrier.Type                     = D3D12_RESOURCE_BARRIER_TYPE_ALIASING;
        barrier.Flags                    = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrier.Aliasing.pResourceBefore = nullptr;
        barrier.Aliasing.pResourceAfter  = nullptr;
        }
      }
    else if(b.buffer!=nullptr) {
      barrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
      barrier.Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;
      barrier.Transition.pResource   = toDxResource(b);
      barrier.Transition.StateBefore = ::nativeFormat(b.prev);
      barrier.Transition.StateAfter  = ::nativeFormat(b.next);
      barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
      if(barrier.Transition.StateBefore==barrier.Transition.StateAfter) {
        barrier.Type          = D3D12_RESOURCE_BARRIER_TYPE_UAV;
        barrier.Flags         = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrier.UAV.pResource = toDxResource(b);
        }
      }
    else {
      barrier.Type                   = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
      barrier.Flags                  = D3D12_RESOURCE_BARRIER_FLAG_NONE;
      barrier.Transition.pResource   = toDxResource(b);
      barrier.Transition.StateBefore = ::nativeFormat(b.prev);
      barrier.Transition.StateAfter  = ::nativeFormat(b.next);
      barrier.Transition.Subresource = (b.mip==uint32_t(-1) ?  D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES : b.mip);
      if(barrier.Transition.StateBefore==barrier.Transition.StateAfter)
        continue;
      }
    ++rbCount;
    }

  if(rbCount>0)
    impl->ResourceBarrier(rbCount,rb);
  }

void DxCommandBuffer::copy(AbstractGraphicsApi::Buffer&  dstBuf, size_t offset,
                           AbstractGraphicsApi::Texture& srcTex, uint32_t width, uint32_t height, uint32_t mip) {
  auto& dst = reinterpret_cast<DxBuffer&>(dstBuf);
  auto& src = reinterpret_cast<DxTexture&>(srcTex);

  const UINT bpp       = src.bitCount()/8;
  const UINT pitchBase = UINT(width)*bpp;
  const UINT pitch     = ((pitchBase+D3D12_TEXTURE_DATA_PITCH_ALIGNMENT-1)/D3D12_TEXTURE_DATA_PITCH_ALIGNMENT)*D3D12_TEXTURE_DATA_PITCH_ALIGNMENT;

  if(pitch==pitchBase && (offset%D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT)==0) {
    resState.setLayout(src,ResourceAccess::Sampler);
    resState.onTranferUsage(src.nonUniqId, dst.nonUniqId, false);
    resState.flush(*this);

    copyNative(dstBuf,offset, srcTex,width,height,mip);
    return;
    }

  // shader emulation
  // TODO: restore bindings, pipeline state
  struct PushUbo {
    int32_t mip     = 0;
    int32_t bitCnt  = 8;
    int32_t compCnt = 4;
    } push;
  push.mip = mip;

  auto&  prog    = copyShader(src.format, push.bitCnt, push.compCnt);
  size_t outSize = (width*height*(push.compCnt*push.bitCnt/8) + sizeof(uint32_t)-1)/sizeof(uint32_t);

  this->setBinding(0, &src, Sampler::nearest(), 0);
  this->setBinding(1, &dst, 0);
  this->setComputePipeline(prog);
  this->setPushData(&push, sizeof(push));
  const size_t maxWG = 65535;
  this->dispatch(std::min(outSize,maxWG),(outSize+maxWG-1)%maxWG,1u);
  }

DxCompPipeline& DxCommandBuffer::copyShader(DXGI_FORMAT format, int32_t& bitCnt, int32_t& compCnt) {
  switch(format) {
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

void DxCommandBuffer::prepareDraw(size_t voffset, size_t firstInstance) {
  if(pushBaseInstanceId!=uint32_t(-1)) {
    struct SPIRV_Cross_VertexInfo {
      int32_t SPIRV_Cross_BaseVertex;
      int32_t SPIRV_Cross_BaseInstance;
      } compat;
    compat.SPIRV_Cross_BaseVertex   = int32_t(voffset);
    compat.SPIRV_Cross_BaseInstance = int32_t(firstInstance);
    impl->SetGraphicsRoot32BitConstants(UINT(pushBaseInstanceId),UINT(2),&compat,0);
    }
  }

void DxCommandBuffer::draw(const AbstractGraphicsApi::Buffer* ivbo, size_t stride, size_t offset, size_t vertexCount,
                           size_t firstInstance, size_t instanceCount) {
  const DxBuffer* vbo = reinterpret_cast<const DxBuffer*>(ivbo);
  if(T_LIKELY(vbo!=nullptr)) {
    D3D12_VERTEX_BUFFER_VIEW view;
    view.BufferLocation = vbo->impl.get()->GetGPUVirtualAddress();
    view.SizeInBytes    = vbo->sizeInBytes;
    view.StrideInBytes  = UINT(stride);
    impl->IASetVertexBuffers(0,1,&view);
    }

  implSetUniforms(PipelineStage::S_Graphics);
  implSetPushData(PipelineStage::S_Graphics);
  prepareDraw(offset, firstInstance);
  impl->DrawInstanced(UINT(vertexCount),UINT(instanceCount),UINT(offset),UINT(firstInstance));
  }

void DxCommandBuffer::drawIndexed(const AbstractGraphicsApi::Buffer* ivbo, size_t stride, size_t voffset,
                                  const AbstractGraphicsApi::Buffer& iibo, Detail::IndexClass cls, size_t ioffset, size_t isize,
                                  size_t firstInstance, size_t instanceCount) {
  const DxBuffer* vbo = reinterpret_cast<const DxBuffer*>(ivbo);
  const DxBuffer& ibo = reinterpret_cast<const DxBuffer&>(iibo);

  if(T_LIKELY(vbo!=nullptr)) {
    D3D12_VERTEX_BUFFER_VIEW view;
    view.BufferLocation = vbo->impl.get()->GetGPUVirtualAddress();
    view.SizeInBytes    = vbo->sizeInBytes;
    view.StrideInBytes  = UINT(stride);
    impl->IASetVertexBuffers(0,1,&view);
    }

  D3D12_INDEX_BUFFER_VIEW iview;
  iview.BufferLocation = ibo.impl.get()->GetGPUVirtualAddress();
  iview.SizeInBytes    = ibo.sizeInBytes;
  iview.Format         = nativeFormat(cls);
  impl->IASetIndexBuffer(&iview);

  implSetUniforms(PipelineStage::S_Graphics);
  implSetPushData(PipelineStage::S_Graphics);
  prepareDraw(voffset, firstInstance);
  impl->DrawIndexedInstanced(UINT(isize),UINT(instanceCount),UINT(ioffset),INT(voffset),UINT(firstInstance));
  }

void DxCommandBuffer::drawIndirect(const AbstractGraphicsApi::Buffer& indirect, size_t offset) {
  const DxBuffer& ind  = reinterpret_cast<const DxBuffer&>(indirect);
  auto&           sign = dev.drawIndirectSgn.get();

  // block future writers
  resState.onUavUsage(ind.nonUniqId, NonUniqResId::I_None, PipelineStage::S_Indirect);
  implSetUniforms(PipelineStage::S_Graphics);
  implSetPushData(PipelineStage::S_Graphics);
  //resState.flush(*this);
  /*
  if(!dev.props.enhancedBarriers && indirectCmd.find(&ind)==indirectCmd.end()) {
    indirectCmd.insert(&ind);
    // UNORDERED_ACCESS to INDIRECT_ARGUMENT (the UMD can assume the UAV writes occurred before the Render Pass)
    barrier(indirect, ResourceAccess::UavReadWriteAll, ResourceAccess::Indirect);
    }
  */

  impl->ExecuteIndirect(sign, 1, ind.impl.get(), UINT64(offset), nullptr, 0);
  }

void DxCommandBuffer::dispatchMesh(size_t x, size_t y, size_t z) {
  implSetUniforms(PipelineStage::S_Graphics);
  implSetPushData(PipelineStage::S_Graphics);
  impl->DispatchMesh(UINT(x),UINT(y),UINT(z));
  }

void DxCommandBuffer::dispatchMeshIndirect(const AbstractGraphicsApi::Buffer& indirect, size_t offset) {
  const DxBuffer& ind  = reinterpret_cast<const DxBuffer&>(indirect);
  auto&           sign = dev.drawMeshIndirectSgn.get();

  // block future writers
  resState.onUavUsage(ind.nonUniqId, NonUniqResId::I_None, PipelineStage::S_Indirect);
  implSetUniforms(PipelineStage::S_Graphics);
  implSetPushData(PipelineStage::S_Graphics);

  /*
  if(!dev.props.enhancedBarriers && indirectCmd.find(&ind)==indirectCmd.end()) {
    indirectCmd.insert(&ind);
    // UNORDERED_ACCESS to INDIRECT_ARGUMENT (the UMD can assume the UAV writes occurred before the Render Pass)
    barrier(indirect, ResourceAccess::UavReadWriteAll, ResourceAccess::Indirect);
    }
  */
  impl->ExecuteIndirect(sign, 1, ind.impl.get(), UINT64(offset), nullptr, 0);
  }

void DxCommandBuffer::copy(AbstractGraphicsApi::Buffer& dstBuf, size_t offsetDest, const AbstractGraphicsApi::Buffer& srcBuf, size_t offsetSrc, size_t size) {
  auto& dst = reinterpret_cast<DxBuffer&>(dstBuf);
  auto& src = reinterpret_cast<const DxBuffer&>(srcBuf);
  // barrier(dstBuf, ResourceAccess::UavRead, ResourceAccess::TransferDst);
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

  resState.onTranferUsage(src.nonUniqId, dst.nonUniqId, false);
  resState.flush(*this);
  impl->CopyTextureRegion(&dstLoc, 0, 0, 0, &srcLoc, nullptr);
  }

void DxCommandBuffer::fill(AbstractGraphicsApi::Texture& dstTex, uint32_t val) {
  std::unique_ptr<FillUAV> dx(new FillUAV(dev,reinterpret_cast<DxTexture&>(dstTex),val));
  pushStage(dx.release());
  }

void DxCommandBuffer::buildBlas(AbstractGraphicsApi::Buffer& bbo, AbstractGraphicsApi::BlasBuildCtx& rtctx, AbstractGraphicsApi::Buffer& scratch) {
  auto& ctx  = reinterpret_cast<DxBlasBuildCtx&>(rtctx);
  auto& dest = *reinterpret_cast<DxBuffer&>(bbo).impl;

  D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS blasInputs = {};
  blasInputs.Type           = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
  blasInputs.Flags          = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
  blasInputs.pGeometryDescs = ctx.geometry.data();
  blasInputs.NumDescs       = UINT(ctx.geometry.size());

  D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC desc = {};
  desc.DestAccelerationStructureData    = dest.GetGPUVirtualAddress();
  desc.Inputs                           = blasInputs;
  desc.SourceAccelerationStructureData  = 0;
  desc.ScratchAccelerationStructureData = reinterpret_cast<DxBuffer&>(scratch).impl->GetGPUVirtualAddress();

  // make sure BLAS'es are ready
  resState.onUavUsage(NonUniqResId::I_None, reinterpret_cast<DxBuffer&>(bbo).nonUniqId, PipelineStage::S_RtAs);
  resState.flush(*this);
  impl->BuildRaytracingAccelerationStructure(&desc,0,nullptr);
  }

void DxCommandBuffer::buildTlas(AbstractGraphicsApi::Buffer& tbo,
                                AbstractGraphicsApi::Buffer& instances, uint32_t numInstances,
                                AbstractGraphicsApi::Buffer& scratch) {
  // NOTE: instances must be 16 bytes aligned
  auto& dest = *reinterpret_cast<DxBuffer&>(tbo).impl;

  D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS tlasInputs = {};
  tlasInputs.Type          = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
  tlasInputs.Flags         = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
  tlasInputs.NumDescs      = numInstances;
  tlasInputs.DescsLayout   = D3D12_ELEMENTS_LAYOUT_ARRAY;
  if(numInstances>0)
    tlasInputs.InstanceDescs = reinterpret_cast<DxBuffer&>(instances).impl->GetGPUVirtualAddress();

  D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC desc = {};
  desc.DestAccelerationStructureData    = dest.GetGPUVirtualAddress();
  desc.Inputs                           = tlasInputs;
  desc.SourceAccelerationStructureData  = 0;
  desc.ScratchAccelerationStructureData = reinterpret_cast<DxBuffer&>(scratch).impl->GetGPUVirtualAddress();

  resState.onUavUsage(NonUniqResId::I_None, reinterpret_cast<DxBuffer&>(tbo).nonUniqId, PipelineStage::S_RtAs);
  resState.flush(*this);
  impl->BuildRaytracingAccelerationStructure(&desc,0,nullptr);
  }

void DxCommandBuffer::pushChunk() {
  if(impl.get()!=nullptr) {
    dxAssert(impl->Close());
    Chunk ch;
    ch.impl = impl.release();
    chunks.push(ch);

    impl = nullptr;
    curHeaps = DxDescriptorArray::CbState{};
    }
  }

void DxCommandBuffer::newChunk() {
  pushChunk();

  dxAssert(dev.device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, pool.get(), nullptr,
                                         uuid<ID3D12GraphicsCommandList6>(), reinterpret_cast<void**>(&impl)));
  //dxAssert(dev.device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, pool.get(), nullptr,
  //                                       _uuidof(ID3D12GraphicsCommandList7), reinterpret_cast<void**>(&impl)));
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

void DxCommandBuffer::generateMipmap(AbstractGraphicsApi::Texture& img,
                                     uint32_t texWidth, uint32_t texHeight, uint32_t mipLevels) {
  if(mipLevels==1)
    return;
/*
  resState.flush(*this);
  Detail::Bindings prev = static_cast<Detail::Bindings&>(bindings);

  auto& shader = *dev.blit.handler;
  impl->SetPipelineState(&shader.instance(reinterpret_cast<DxTexture&>(img).format));
  impl->SetGraphicsRootSignature(shader.sign.get());
  impl->IASetPrimitiveTopology(shader.topology);
  impl->IASetVertexBuffers(0,0,nullptr);

  D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle  = rtvHeap->GetCPUDescriptorHandleForHeapStart();
  auto                        rtvHeapInc = dev.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);


  uint32_t w = texWidth;
  uint32_t h = texHeight;
  resState.setLayout(img, ResourceAccess::ColorAttach);
  for(uint32_t i=1; i<mipLevels; ++i) {
    const int dstW = (w==1 ? 1 : w/2);
    const int dstH = (h==1 ? 1 : h/2);

    barrier(img,ResourceAccess::ColorAttach,ResourceAccess::Sampler,i-1);
    impl->OMSetRenderTargets(1, &rtvHandle, TRUE, nullptr);

    setViewport(Rect(0, 0, dstW, dstH));
    setScissor(Rect(0, 0, dstW, dstH));

    setBinding(0, &img, Sampler::bilinear(), i-1);
    implSetUniforms(S_Graphics);

    impl->DrawInstanced(6,1,0,0);

    w             = dstW;
    h             = dstH;
    rtvHandle.ptr+= rtvHeapInc;
    }
  barrier(img, ResourceAccess::ColorAttach, ResourceAccess::Sampler, mipLevels-1);

  static_cast<Detail::Bindings&>(bindings) = prev;
  bindings.durty = true;
  */
  // std::unique_ptr<MipMaps> dx(new MipMaps(dev,reinterpret_cast<DxTexture&>(img),texWidth,texHeight,mipLevels));
  // pushStage(dx.release());
  }

void DxCommandBuffer::copyNative(AbstractGraphicsApi::Buffer& dstBuf, size_t offset,
                                 const AbstractGraphicsApi::Texture& srcTex, uint32_t width, uint32_t height, uint32_t mip) {
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

#endif


