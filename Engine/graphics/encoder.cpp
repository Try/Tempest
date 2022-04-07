#include "encoder.h"

#include <Tempest/Attachment>
#include <Tempest/ZBuffer>
#include <Tempest/Texture2d>

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

void Encoder<Tempest::CommandBuffer>::setUniforms(const RenderPipeline& p, const DescriptorSet &ubo, const void* data, size_t sz) {
  setUniforms(p);
  if(sz>0)
    impl->setBytes(*p.impl.handler,data,sz);
  if(ubo.impl.handler!=&DescriptorSet::emptyDesc)
    impl->setUniforms(*p.impl.handler,*ubo.impl.handler);
  }

void Encoder<Tempest::CommandBuffer>::setUniforms(const RenderPipeline& p, const void* data, size_t sz) {
  setUniforms(p);
  if(sz>0)
    impl->setBytes(*p.impl.handler,data,sz);
  }

void Encoder<Tempest::CommandBuffer>::setUniforms(const RenderPipeline& p, const DescriptorSet &ubo) {
  setUniforms(p);
  if(ubo.impl.handler!=&DescriptorSet::emptyDesc)
    impl->setUniforms(*p.impl.handler,*ubo.impl.handler);
  }

void Encoder<Tempest::CommandBuffer>::setUniforms(const RenderPipeline &p) {
  if(state.stage!=Rendering)
    throw std::system_error(Tempest::GraphicsErrc::DrawCallWithoutFbo);
  if(state.curPipeline!=p.impl.handler) {
    impl->setPipeline(*p.impl.handler);
    state.curPipeline = p.impl.handler;
    }
  }

void Encoder<Tempest::CommandBuffer>::setUniforms(const ComputePipeline& p, const DescriptorSet &ubo, const void* data, size_t sz) {
  setUniforms(p);
  impl->setBytes(*p.impl.handler,data,sz);
  if(ubo.impl.handler!=&DescriptorSet::emptyDesc)
    impl->setUniforms(*p.impl.handler,*ubo.impl.handler);
  }

void Encoder<Tempest::CommandBuffer>::setUniforms(const ComputePipeline& p, const void* data, size_t sz) {
  setUniforms(p);
  impl->setBytes(*p.impl.handler,data,sz);
  }

void Encoder<Tempest::CommandBuffer>::setUniforms(const ComputePipeline& p, const DescriptorSet &ubo) {
  setUniforms(p);
  if(ubo.impl.handler!=&DescriptorSet::emptyDesc)
    impl->setUniforms(*p.impl.handler,*ubo.impl.handler);
  }

void Encoder<Tempest::CommandBuffer>::setUniforms(const ComputePipeline& p) {
  if(state.stage==Rendering)
    throw std::system_error(Tempest::GraphicsErrc::ComputeCallInRenderPass);
  if(state.curCompute!=p.impl.handler) {
    impl->setComputePipeline(*p.impl.handler);
    state.curCompute  = p.impl.handler;
    state.curPipeline = nullptr;
    state.stage       = Compute;
    }
  }

void Encoder<Tempest::CommandBuffer>::implDraw(const VideoBuffer& vbo, size_t offset, size_t size, size_t firstInstance, size_t instanceCount) {
  if(state.stage!=Rendering)
    throw std::system_error(Tempest::GraphicsErrc::DrawCallWithoutFbo);
  if(!vbo.impl)
    return;
  impl->draw(*vbo.impl.handler,offset,size,firstInstance,instanceCount);
  }

void Encoder<Tempest::CommandBuffer>::implDraw(const VideoBuffer &vbo, const VideoBuffer &ibo, Detail::IndexClass icls, size_t offset, size_t size,
                                               size_t firstInstance, size_t instanceCount) {
  if(state.stage!=Rendering)
    throw std::system_error(Tempest::GraphicsErrc::DrawCallWithoutFbo);
  if(!vbo.impl || !ibo.impl)
    return;
  impl->drawIndexed(*vbo.impl.handler,0,*ibo.impl.handler,icls,offset,size, firstInstance,instanceCount);
  }

void Encoder<CommandBuffer>::dispatch(size_t x, size_t y, size_t z) {
  if(state.stage==Rendering)
    throw std::system_error(Tempest::GraphicsErrc::ComputeCallInRenderPass);
  impl->dispatch(x,y,z);
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
  uint32_t      w = uint32_t(rt[0].attachment->w()); // FIXME: handle z-only passes
  uint32_t      h = uint32_t(rt[0].attachment->h());

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
  auto& tx = *textureCast(src).impl.handler;
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
  impl->generateMipmap(*textureCast(tex).impl.handler,w,h,mipCount(w,h));
  }
