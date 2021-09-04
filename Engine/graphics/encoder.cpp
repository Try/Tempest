#include "encoder.h"

#include <Tempest/Attachment>
#include <Tempest/FrameBuffer>
#include <Tempest/RenderPass>
#include <Tempest/Texture2d>

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
  impl->end();
  }

void Encoder<Tempest::CommandBuffer>::setViewport(int x, int y, int w, int h) {
  impl->setViewport(Rect(x,y,w,h));
  }

void Encoder<Tempest::CommandBuffer>::setViewport(const Rect &vp) {
  impl->setViewport(vp);
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
  if(curPass.fbo==nullptr)
    throw std::system_error(Tempest::GraphicsErrc::DrawCallWithoutFbo);
  if(state.curPipeline!=p.impl.handler) {
    impl->setPipeline(*p.impl.handler);
    state.curPipeline=p.impl.handler;
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
  if(curPass.fbo!=nullptr)
    throw std::system_error(Tempest::GraphicsErrc::ComputeCallInRenderPass);
  if(state.curCompute!=p.impl.handler) {
    impl->setComputePipeline(*p.impl.handler);
    state.curCompute  = p.impl.handler;
    state.curPipeline = nullptr;
    }
  }

void Encoder<Tempest::CommandBuffer>::implDraw(const VideoBuffer& vbo, size_t offset, size_t size, size_t firstInstance, size_t instanceCount) {
  if(curPass.fbo==nullptr)
    throw std::system_error(Tempest::GraphicsErrc::DrawCallWithoutFbo);
  if(!vbo.impl)
    return;
  impl->draw(*vbo.impl.handler,offset,size,firstInstance,instanceCount);
  }

void Encoder<Tempest::CommandBuffer>::implDraw(const VideoBuffer &vbo, const VideoBuffer &ibo, Detail::IndexClass index, size_t offset, size_t size,
                                               size_t firstInstance, size_t instanceCount) {
  if(curPass.fbo==nullptr)
    throw std::system_error(Tempest::GraphicsErrc::DrawCallWithoutFbo);
  if(!vbo.impl || !ibo.impl)
    return;
  impl->drawIndexed(*vbo.impl.handler,*ibo.impl.handler,index,offset,size,0,firstInstance,instanceCount);
  }

void Encoder<CommandBuffer>::dispatch(size_t x, size_t y, size_t z) {
  if(curPass.fbo!=nullptr)
    throw std::system_error(Tempest::GraphicsErrc::ComputeCallInRenderPass);
  impl->dispatch(x,y,z);
  }

void Encoder<CommandBuffer>::setFramebuffer(std::nullptr_t) {
  setFramebuffer(FrameBuffer(),RenderPass());
  }

void Encoder<CommandBuffer>::setFramebuffer(const FrameBuffer &fbo, const RenderPass &p) {
  implEndRenderPass();

  if(fbo.impl.handler==nullptr && p.impl.handler==nullptr) {
    state.curPipeline = nullptr;
    return;
    }

  impl->beginRenderPass(fbo.impl.handler,p.impl.handler, fbo.w(),fbo.h());
  curPass.fbo      = &fbo;
  curPass.pass     = &p;
  state.curCompute = nullptr;
  }

void Encoder<CommandBuffer>::implEndRenderPass() {
  if(curPass.pass!=nullptr) {
    state.curPipeline = nullptr;
    curPass           = Pass();
    impl->endRenderPass();
    }
  }

void Encoder<CommandBuffer>::copy(const Attachment& src, uint32_t mip, StorageBuffer& dest, size_t offset) {
  if(offset%4!=0)
    throw std::system_error(Tempest::GraphicsErrc::InvalidStorageBuffer);
  uint32_t w = src.w(), h = src.h();
  impl->copy(*dest.impl.impl.handler,TextureLayout::Sampler,w,h,mip,*textureCast(src).impl.handler,offset);
  }

void Encoder<CommandBuffer>::copy(const Texture2d& src, uint32_t mip, StorageBuffer& dest, size_t offset) {
  if(offset%4!=0)
    throw std::system_error(Tempest::GraphicsErrc::InvalidStorageBuffer);
  uint32_t w = src.w(), h = src.h();
  impl->copy(*dest.impl.impl.handler,TextureLayout::Sampler,w,h,mip,*src.impl.handler,offset);
  }

void Encoder<CommandBuffer>::generateMipmaps(Attachment& tex) {
  uint32_t w = tex.w(), h = tex.h();
  impl->generateMipmap(*textureCast(tex).impl.handler,TextureLayout::Sampler,w,h,mipCount(w,h));
  }
