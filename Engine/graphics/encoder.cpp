#include "encoder.h"

#include <Tempest/Attachment>
#include <Tempest/ZBuffer>
#include <Tempest/Texture2d>
#include <Tempest/StorageBuffer>
#include <cassert>

#include "utility/compiller_hints.h"

using namespace Tempest;

static uint32_t mipCount(uint32_t w, uint32_t h) {
  uint32_t s = std::max(w,h);
  uint32_t n = 1;
  while(s>1) {
    ++n;
    s = s/2;
    }
  return n;
  }

Encoder<Tempest::CommandBuffer>::Encoder(Tempest::CommandBuffer* ow)
  :impl(ow->impl.handler) {
  impl->begin();
  }

Encoder<CommandBuffer>::Encoder(Encoder<CommandBuffer> &&e)
  :impl(e.impl),state(std::move(e.state)) {
  e.impl  = nullptr;
  }

Encoder<CommandBuffer> &Encoder<CommandBuffer>::operator =(Encoder<CommandBuffer> &&e) {
  impl   = e.impl;
  state  = std::move(e.state);

  e.impl = nullptr;
  return *this;
  }

Encoder<Tempest::CommandBuffer>::~Encoder() noexcept(false) {
  if(impl==nullptr)
    return;
  if(state.stage==Rendering)
    impl->endRendering();
  impl->end();
  }

void Encoder<Tempest::CommandBuffer>::setViewport(int x, int y, int w, int h) {
  impl->setViewport(Rect(x,y,w,h));
  }

void Encoder<Tempest::CommandBuffer>::setViewport(const Rect &vp) {
  impl->setViewport(vp);
  }

void Encoder<Tempest::CommandBuffer>::setScissor(int x,int y,int w,int h) {
  impl->setScissor(Rect(x,y,w,h));
  }

void Encoder<Tempest::CommandBuffer>::setScissor(const Rect &vp) {
  impl->setScissor(vp);
  }

void Encoder<Tempest::CommandBuffer>::setDebugMarker(std::string_view tag) {
  impl->setDebugMarker(tag);
  }

void Encoder<Tempest::CommandBuffer>::setPushData(const void* data, size_t size) {
  impl->setPushData(data, size);
  }

void Encoder<Tempest::CommandBuffer>::setBinding(size_t id, const Texture2d& tex, const Sampler& smp) {
  if(!tex.impl.handler)
    throw std::system_error(Tempest::GraphicsErrc::InvalidTexture);
  impl->setBinding(id, tex.impl.handler, uint32_t(-1), ComponentMapping(), smp);
  }

void Encoder<Tempest::CommandBuffer>::setBinding(size_t id, const Texture2d& tex, const ComponentMapping& m, const Sampler& smp) {
  if(!tex.impl.handler)
    throw std::system_error(Tempest::GraphicsErrc::InvalidTexture);
  impl->setBinding(id, tex.impl.handler, uint32_t(-1), m, smp);
  }

void Encoder<Tempest::CommandBuffer>::setBinding(size_t id, const Attachment& tex, const Sampler& smp) {
  if(!tex.tImpl.impl.handler)
    throw std::system_error(Tempest::GraphicsErrc::InvalidTexture);
  impl->setBinding(id, tex.tImpl.impl.handler, uint32_t(-1), ComponentMapping(), smp);
  }

void Encoder<Tempest::CommandBuffer>::setBinding(size_t id, const ZBuffer& tex, const Sampler& smp) {
  if(!tex.tImpl.impl.handler)
    throw std::system_error(Tempest::GraphicsErrc::InvalidTexture);
  impl->setBinding(id, tex.tImpl.impl.handler, uint32_t(-1), ComponentMapping(), smp);
  }

void Encoder<Tempest::CommandBuffer>::setBinding(size_t id, const StorageImage& tex, const Sampler& smp, uint32_t mipLevel) {
  if(!tex.tImpl.impl.handler)
    throw std::system_error(Tempest::GraphicsErrc::InvalidTexture);
  impl->setBinding(id, tex.tImpl.impl.handler, mipLevel, ComponentMapping(), smp);
  }

void Encoder<Tempest::CommandBuffer>::setBinding(size_t id, const StorageBuffer& buf, size_t offset) {
  if(buf.impl.impl.handler==nullptr && offset!=0)
    throw std::system_error(Tempest::GraphicsErrc::InvalidUniformBuffer);
  impl->setBinding(id, buf.impl.impl.handler, offset);
  }

void Encoder<Tempest::CommandBuffer>::setBinding(size_t id, const Detail::ResourcePtr<Texture2d>& tex, const Sampler& smp) {
  if(!tex.impl.handler)
    throw std::system_error(Tempest::GraphicsErrc::InvalidTexture);
  impl->setBinding(id, tex.impl.handler, uint32_t(-1), ComponentMapping(), smp);
  }

void Encoder<Tempest::CommandBuffer>::implBindBuffer(size_t id, const Detail::VideoBuffer& buf) {
  if(buf.impl.handler==nullptr)
    throw std::system_error(Tempest::GraphicsErrc::InvalidUniformBuffer);
  impl->setBinding(id, buf.impl.handler, 0);
  }

void Encoder<Tempest::CommandBuffer>::setBinding(size_t id, const DescriptorArray& arr) {
  if(arr.impl.handler==nullptr)
    throw std::system_error(Tempest::GraphicsErrc::InvalidUniformBuffer); //TODO: proper exception
  impl->setBinding(id, arr.impl.handler);
  }

void Encoder<Tempest::CommandBuffer>::setBinding(size_t id, const Sampler &smp) {
  impl->setBinding(id, smp);
  }

void Encoder<Tempest::CommandBuffer>::setBinding(size_t id, const AccelerationStructure& tlas) {
  if(tlas.impl.handler)
    impl->setBinding(id, tlas.impl.handler); else
    throw std::system_error(Tempest::GraphicsErrc::InvalidAccelerationStructure);
  }

void Encoder<Tempest::CommandBuffer>::setPipeline(const RenderPipeline& p) {
  if(state.stage!=Rendering)
    throw std::system_error(Tempest::GraphicsErrc::DrawCallWithoutFbo);
  assert(p.impl.handler);
  if(state.curPipeline!=p.impl.handler) {
    impl->setPipeline(*p.impl.handler);
    state.curPipeline = p.impl.handler;
    }
  }

void Encoder<Tempest::CommandBuffer>::setPipeline(const ComputePipeline& p) {
  if(state.stage==Rendering)
    throw std::system_error(Tempest::GraphicsErrc::ComputeCallInRenderPass);
  assert(p.impl.handler);
  if(state.curCompute!=p.impl.handler) {
    impl->setComputePipeline(*p.impl.handler);
    state.curCompute  = p.impl.handler;
    state.curPipeline = nullptr;
    state.stage       = Compute;
    }
  }

void Encoder<Tempest::CommandBuffer>::implDraw(const Detail::VideoBuffer& vbo, size_t stride, size_t offset, size_t size, size_t firstInstance, size_t instanceCount) {
  if(state.stage!=Rendering)
    throw std::system_error(Tempest::GraphicsErrc::DrawCallWithoutFbo);
  if(size==0)
    return;
  impl->draw(vbo.impl.handler,stride,offset,size,firstInstance,instanceCount);
  }

void Encoder<Tempest::CommandBuffer>::implDraw(const Detail::VideoBuffer &vbo, size_t stride, const Detail::VideoBuffer &ibo, Detail::IndexClass icls, size_t offset, size_t size,
                                               size_t firstInstance, size_t instanceCount) {
  if(state.stage!=Rendering)
    throw std::system_error(Tempest::GraphicsErrc::DrawCallWithoutFbo);
  if(size==0 || !ibo.impl)
    return;
  impl->drawIndexed(vbo.impl.handler,stride,0,*ibo.impl.handler,icls,offset,size, firstInstance,instanceCount);
  }

void Encoder<Tempest::CommandBuffer>::drawIndirect(const StorageBuffer& indirect, size_t offset) {
  if(state.stage!=Rendering)
    throw std::system_error(Tempest::GraphicsErrc::DrawCallWithoutFbo);
  if(offset%4 != 0)
    throw std::system_error(Tempest::GraphicsErrc::InvalidStorageBuffer); //TODO: error code
  impl->drawIndirect(*indirect.impl.impl.handler, offset);
  }

void Encoder<Tempest::CommandBuffer>::dispatchMesh(size_t x, size_t y, size_t z) {
  if(state.stage!=Rendering)
    throw std::system_error(Tempest::GraphicsErrc::DrawCallWithoutFbo);
  impl->dispatchMesh(x,y,z);
  }

void Encoder<Tempest::CommandBuffer>::dispatchMeshIndirect(const StorageBuffer& indirect, size_t offset) {
  if(state.stage!=Rendering)
    throw std::system_error(Tempest::GraphicsErrc::DrawCallWithoutFbo);
  if(offset%4 != 0)
    throw std::system_error(Tempest::GraphicsErrc::InvalidStorageBuffer);
  impl->dispatchMeshIndirect(*indirect.impl.impl.handler, offset);
  }

void Encoder<Tempest::CommandBuffer>::dispatchMeshThreads(size_t x, size_t y, size_t z) {
  if(state.stage!=Rendering)
    throw std::system_error(Tempest::GraphicsErrc::DrawCallWithoutFbo);
  auto sz = state.curPipeline->workGroupSize();
  x = (x+sz.x-1)/sz.x;
  y = (y+sz.y-1)/sz.y;
  z = (z+sz.z-1)/sz.z;
  impl->dispatchMesh(x,y,z);
  }

void Encoder<Tempest::CommandBuffer>::dispatchMeshThreads(Size sz) {
  dispatchMeshThreads(size_t(sz.w), size_t(sz.h), 1);
  }

void Encoder<CommandBuffer>::dispatch(size_t x, size_t y, size_t z) {
  if(state.stage==Rendering)
    throw std::system_error(Tempest::GraphicsErrc::ComputeCallInRenderPass);
  impl->dispatch(x,y,z);
  }

void Encoder<Tempest::CommandBuffer>::dispatchThreads(size_t x, size_t y, size_t z) {
  if(state.stage==Rendering)
    throw std::system_error(Tempest::GraphicsErrc::ComputeCallInRenderPass);
  auto sz = state.curCompute->workGroupSize();
  x = (x+sz.x-1)/sz.x;
  y = (y+sz.y-1)/sz.y;
  z = (z+sz.z-1)/sz.z;
  impl->dispatch(x,y,z);
  }

void Encoder<Tempest::CommandBuffer>::dispatchThreads(Size sz) {
  dispatchThreads(size_t(sz.w), size_t(sz.h), 1u);
  }

void Encoder<CommandBuffer>::dispatchIndirect(const StorageBuffer& indirect, size_t offset) {
  if (offset % 4 != 0)
    throw std::system_error(Tempest::GraphicsErrc::InvalidStorageBuffer);
  impl->dispatchIndirect(*indirect.impl.impl.handler, offset);
  }

void Encoder<CommandBuffer>::setFramebuffer(std::initializer_list<AttachmentDesc> rd, AttachmentDesc zd) {
  implSetFramebuffer(rd.begin(),rd.size(),&zd);
  }

void Encoder<CommandBuffer>::setFramebuffer(std::initializer_list<AttachmentDesc> rd) {
  if(T_LIKELY(rd.size()>0)) {
    implSetFramebuffer(rd.begin(),rd.size(),nullptr);
    return;
    }
  // rd.size==0 -> compute
  if(state.stage!=Rendering)
    return;
  if(state.stage==Rendering)
    impl->endRendering();
  state.curPipeline = nullptr;
  state.curCompute  = nullptr;
  state.stage       = None;
  }

void Tempest::Encoder<Tempest::CommandBuffer>::implSetFramebuffer(const AttachmentDesc* rt, size_t rtSize,
                                                                  const AttachmentDesc* zd) {
  if(state.stage==Rendering)
    impl->endRendering();

  if((rtSize+(zd ? 1 : 0)) > MaxFramebufferAttachments)
    throw IncompleteFboException();

  TextureFormat frm[MaxFramebufferAttachments+1] = {};
  uint32_t      w, h;
  if(T_UNLIKELY(rtSize==0)) {
    w = uint32_t(zd->zbuffer->w());
    h = uint32_t(zd->zbuffer->h());
    } else {
    w = uint32_t(rt[0].attachment->w());
    h = uint32_t(rt[0].attachment->h());
    }

  AttachmentDesc                  desc [MaxFramebufferAttachments] = {};
  AbstractGraphicsApi::Texture*   att  [MaxFramebufferAttachments] = {};
  AbstractGraphicsApi::Swapchain* sw   [MaxFramebufferAttachments] = {};
  uint32_t                        imgId[MaxFramebufferAttachments] = {};

  for(size_t i=0; i<rtSize; ++i) {
    Attachment* ax = rt[i].attachment;
    if(ax->w()!=int(w) || ax->h()!=int(h))
      throw IncompleteFboException();

    desc[i] = rt[i];
    if(ax->sImpl.swapchain!=nullptr) {
      frm[i]   = TextureFormat::Undefined;
      sw[i]    = ax->sImpl.swapchain;
      imgId[i] = ax->sImpl.id;
      } else {
      frm[i]   = ax->tImpl.frm;
      att[i]   = ax->tImpl.impl.handler;
      }
    }

  if(zd!=nullptr) {
    if(zd->zbuffer->w()!=int(w) || zd->zbuffer->h()!=int(h))
      throw IncompleteFboException();
    desc[rtSize] = *zd;
    frm [rtSize] = zd->zbuffer->tImpl.frm;
    att [rtSize] = zd->zbuffer->tImpl.impl.handler;
    }

  impl->beginRendering(desc,rtSize+(zd ? 1 : 0),w,h,
                       frm,att,sw,imgId);
  state.stage      = Rendering;
  state.curPipeline = nullptr;
  }

void Encoder<CommandBuffer>::copy(const Attachment& src, uint32_t mip, StorageBuffer& dest, size_t offset) {
  if(offset%4!=0)
    throw std::system_error(Tempest::GraphicsErrc::InvalidStorageBuffer);
  uint32_t w = src.w(), h = src.h();
  auto& tx = *textureCast<const Texture2d&>(src).impl.handler;
  impl->copy(*dest.impl.impl.handler,offset,tx,w,h,mip);
  }

void Encoder<CommandBuffer>::copy(const Texture2d& src, uint32_t mip, StorageBuffer& dest, size_t offset) {
  if(offset%4!=0)
    throw std::system_error(Tempest::GraphicsErrc::InvalidStorageBuffer);
  uint32_t w = src.w(), h = src.h();
  auto& tx = *src.impl.handler;
  impl->copy(*dest.impl.impl.handler,offset,tx,w,h,mip);
  }

void Encoder<CommandBuffer>::generateMipmaps(Attachment& tex) {
  uint32_t w = tex.w(), h = tex.h();
  impl->generateMipmap(*textureCast<Texture2d&>(tex).impl.handler,w,h,mipCount(w,h));
  }
