#if defined(TEMPEST_BUILD_DIRECTX12)

#include "dxcommandbuffer.h"
#include "dxbuffer.h"
#include "dxtexture.h"
#include "dxdevice.h"
#include "dxswapchain.h"
#include "dxdescriptorarray.h"
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
  cmd.BeginEvent(meta, buf, UINT(std::wcslen(buf)*sizeof(wchar_t)));
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
    DxBuffer& buf = *reinterpret_cast<DxBuffer*>(b.buffer);
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

    desc.emplace_back(*dev.blitLayout.handler);
    DxDescriptorArray& ubo = this->desc.back();
    ubo.set(0,&img,Sampler::bilinear(),srcMip);
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

struct DxCommandBuffer::CopyBuf : Stage {
  CopyBuf(DxDevice& dev,
          DxBuffer& dst, size_t offset,
          DxTexture& src, size_t width, size_t height, size_t mip)
    :dst(dst), offset(offset), src(src), width(width), height(height), mip(int32_t(mip)), desc(*dev.copyLayout.handler) {
    }

  void exec(DxCommandBuffer& cmd) override {
    struct PushUbo {
      int32_t mip     = 0;
      int32_t bitCnt  = 8;
      int32_t compCnt = 4;
      } push;
    push.mip = mip;

    auto&  prog    = shader(cmd,push.bitCnt,push.compCnt);
    size_t outSize = (width*height*(push.compCnt*push.bitCnt/8) + sizeof(uint32_t)-1)/sizeof(uint32_t);

    desc.set(0,&src,Sampler::nearest(),0);
    desc.set(1,&dst,0);

    cmd.setComputePipeline(prog);
    cmd.setUniforms(prog,desc);
    cmd.setBytes(prog,&push,sizeof(push));
    const size_t maxWG = 65535;
    cmd.dispatch(std::min(outSize,maxWG),(outSize+maxWG-1)%maxWG,1u);
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
  DxTexture&        src;
  const size_t      width  = 0;
  const size_t      height = 0;
  const int32_t     mip    = 0;

  DxDescriptorArray desc;
  };

struct DxCommandBuffer::FillUAV : Stage {
  using Allocation = DxDescriptorAllocator::Allocation;

  FillUAV(DxDevice& dev, DxTexture& image, uint32_t val)
    :dst(image), val(val) {
    auto& allocator = dev.descAlloc;
    gpu = allocator.alloc    (dst.mips, false);
    cpu = allocator.allocHost(dst.mips);

    hSize = dev.device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    for(uint32_t i=0; i<dst.mips; ++i) {
      D3D12_UNORDERED_ACCESS_VIEW_DESC desc = {};
      uint32_t mipLevel = 0;
      desc.Format = dst.format;
      if(dst.sliceCnt>1) {
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
    for(uint32_t i=0; i<dst.mips; ++i) {
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
  : dev(d) {
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
  resetDone   = true;
  }

bool DxCommandBuffer::isRecording() const {
  return state!=NoRecording;
  }

void DxCommandBuffer::beginRendering(const AttachmentDesc* desc, size_t descSize, uint32_t w, uint32_t h,
                                     const TextureFormat* frm, AbstractGraphicsApi::Texture** att,
                                     AbstractGraphicsApi::Swapchain** sw, const uint32_t* imgId) {
  resState.joinWriters(PipelineStage::S_Graphics);
  resState.setRenderpass(*this,desc,descSize,frm,att,sw,imgId);

  if(state!=Idle) {
    newChunk();
    }

  D3D12_RENDER_PASS_RENDER_TARGET_DESC view[MaxFramebufferAttachments] = {};
  UINT                                 viewSz = 0;
  D3D12_RENDER_PASS_DEPTH_STENCIL_DESC zdesc  = {};
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
      zdesc.cpuDescriptor = t.handle;
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
    impl->BeginRenderPass(viewSz, view, nullptr, D3D12_RENDER_PASS_FLAG_NONE);
    } else {
    impl->BeginRenderPass(viewSz, view, &zdesc,  D3D12_RENDER_PASS_FLAG_NONE);
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
  state = Compute;
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

void DxCommandBuffer::dispatch(size_t x, size_t y, size_t z) {
  curUniforms->ssboBarriers(resState,PipelineStage::S_Compute);
  resState.flush(*this);
  impl->Dispatch(UINT(x),UINT(y),UINT(z));
  }

void DxCommandBuffer::dispatchIndirect(const AbstractGraphicsApi::Buffer& indirect, size_t offset) {
    throw std::runtime_error("dispatch indirect is not implemented for dx12");
  }

void DxCommandBuffer::setPipeline(Tempest::AbstractGraphicsApi::Pipeline& p) {
  DxPipeline& px = reinterpret_cast<DxPipeline&>(p);
  pushBaseInstanceId = px.pushBaseInstanceId;

  impl->SetPipelineState(&px.instance(fboLayout));
  impl->SetGraphicsRootSignature(px.sign.get());
  impl->IASetPrimitiveTopology(px.topology);
  }

void DxCommandBuffer::setBytes(AbstractGraphicsApi::Pipeline& p, const void* data, size_t size) {
  auto& px = reinterpret_cast<DxPipeline&>(p);
  impl->SetGraphicsRoot32BitConstants(UINT(px.pushConstantId),UINT(size/4),data,0);
  }

void DxCommandBuffer::setUniforms(AbstractGraphicsApi::Pipeline& /*p*/, AbstractGraphicsApi::Desc& u) {
  u.ssboBarriers(resState,PipelineStage::S_Graphics);
  implSetUniforms(u,false);
  }

void DxCommandBuffer::implSetUniforms(AbstractGraphicsApi::Desc& u, bool isCompute) {
  DxDescriptorArray& ux = reinterpret_cast<DxDescriptorArray&>(u);
  curUniforms = &ux;
  ux.bind(*impl, curHeaps, isCompute);
  }

void DxCommandBuffer::restoreIndirect() {
  for(auto& i:indirectCmd) {
      issueExplicitIndirectToCommonStateTransition(i);
    }
  indirectCmd.clear();
  }

void DxCommandBuffer::barrier(const AbstractGraphicsApi::BarrierDesc* desc, size_t cnt) {
  D3D12_RESOURCE_BARRIER rb[MaxBarriers];
  uint32_t               rbCount = 0;

  //D3D12_BARRIER_GROUP bar;
  //impl->Barrier(1, &bar);

  for(size_t i=0; i<cnt; ++i) {
    auto& b = desc[i];
    D3D12_RESOURCE_BARRIER& barrier = rb[rbCount];
    if(b.buffer==nullptr && b.texture==nullptr && b.swapchain==nullptr) {
      barrier.Type                    = D3D12_RESOURCE_BARRIER_TYPE_UAV;
      barrier.Flags                   = D3D12_RESOURCE_BARRIER_FLAG_NONE;
      barrier.UAV.pResource           = nullptr;
      }
    else if(b.buffer!=nullptr) {
      barrier.Type                    = D3D12_RESOURCE_BARRIER_TYPE_UAV;
      barrier.Flags                   = D3D12_RESOURCE_BARRIER_FLAG_NONE;
      barrier.UAV.pResource           = toDxResource(b);
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

  std::unique_ptr<CopyBuf> dx(new CopyBuf(dev,dst,offset,src,width,height,mip));
  pushStage(dx.release());
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
  prepareDraw(offset, firstInstance);
  const DxBuffer* vbo = reinterpret_cast<const DxBuffer*>(ivbo);
  if(T_LIKELY(vbo!=nullptr)) {
    D3D12_VERTEX_BUFFER_VIEW view;
    view.BufferLocation = vbo->impl.get()->GetGPUVirtualAddress();
    view.SizeInBytes    = vbo->sizeInBytes;
    view.StrideInBytes  = UINT(stride);
    impl->IASetVertexBuffers(0,1,&view);
    }
  impl->DrawInstanced(UINT(vertexCount),UINT(instanceCount),UINT(offset),UINT(firstInstance));
  }

void DxCommandBuffer::drawIndexed(const AbstractGraphicsApi::Buffer* ivbo, size_t stride, size_t voffset,
                                  const AbstractGraphicsApi::Buffer& iibo, Detail::IndexClass cls, size_t ioffset, size_t isize,
                                  size_t firstInstance, size_t instanceCount) {
  prepareDraw(voffset, firstInstance);
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

  impl->DrawIndexedInstanced(UINT(isize),UINT(instanceCount),UINT(ioffset),INT(voffset),UINT(firstInstance));
  }

void DxCommandBuffer::drawIndirect(const AbstractGraphicsApi::Buffer& indirect, size_t offset) {
  const DxBuffer& ind  = reinterpret_cast<const DxBuffer&>(indirect);
  auto&           sign = dev.drawIndirectSgn.get();

  // block future writers
  resState.onUavUsage(ind.nonUniqId, NonUniqResId::I_None, PipelineStage::S_Indirect);
  //resState.flush(*this);

  if(true && indirectCmd.find(ind.impl.get())==indirectCmd.end()) {
    indirectCmd.insert(ind.impl.get());
    issueExplicitCommonToIndirectStateTransition(ind.impl.get());
    }

  impl->ExecuteIndirect(sign, 1, ind.impl.get(), UINT64(offset), nullptr, 0);
  }

void DxCommandBuffer::dispatchMesh(size_t x, size_t y, size_t z) {
  impl->DispatchMesh(UINT(x),UINT(y),UINT(z));
  }

void DxCommandBuffer::dispatchMeshIndirect(const AbstractGraphicsApi::Buffer& indirect, size_t offset) {
  const DxBuffer& ind  = reinterpret_cast<const DxBuffer&>(indirect);
  auto&           sign = dev.drawMeshIndirectSgn.get();

  // block future writers
  resState.onUavUsage(ind.nonUniqId, NonUniqResId::I_None, PipelineStage::S_Indirect);

  if(true && indirectCmd.find(ind.impl.get())==indirectCmd.end()) {
    indirectCmd.insert(ind.impl.get());
    issueExplicitCommonToIndirectStateTransition(ind.impl.get());
    }
  impl->ExecuteIndirect(sign, 1, ind.impl.get(), UINT64(offset), nullptr, 0);
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

  resState.flush(*this);

  std::unique_ptr<MipMaps> dx(new MipMaps(dev,reinterpret_cast<DxTexture&>(img),texWidth,texHeight,mipLevels));
  pushStage(dx.release());
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

void DxCommandBuffer::issueExplicitResourceStateTransition(ID3D12Resource* buf, D3D12_RESOURCE_STATES stateBefore, D3D12_RESOURCE_STATES stateAfter)
{
  D3D12_RESOURCE_BARRIER barrier;
  barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
  barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
  barrier.Transition.pResource = buf;
  barrier.Transition.StateAfter = stateAfter;
  barrier.Transition.StateBefore = stateBefore;
  barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
  impl->ResourceBarrier(1, &barrier);
  }

void DxCommandBuffer::issueExplicitCommonToIndirectStateTransition(ID3D12Resource* buf)
{
  issueExplicitResourceStateTransition(buf, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT);
  }

void DxCommandBuffer::issueExplicitIndirectToCommonStateTransition(ID3D12Resource* buf)
{
  issueExplicitResourceStateTransition(buf, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT, D3D12_RESOURCE_STATE_COMMON);
  }

#endif


