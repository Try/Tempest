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
  :owner(ow),impl(ow->impl.handler) {
  state.vp.width  = 0;
  state.vp.height = 0;

  impl->begin();
  }

Encoder<CommandBuffer>::Encoder(Encoder<CommandBuffer> &&e)
  :owner(e.owner),impl(e.impl),state(std::move(e.state)) {
  e.owner = nullptr;
  e.impl  = nullptr;
  }

Encoder<CommandBuffer> &Encoder<CommandBuffer>::operator =(Encoder<CommandBuffer> &&e) {
  owner = e.owner;
  impl  = e.impl;
  state = std::move(e.state);

  e.owner = nullptr;
  e.impl  = nullptr;

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

void Encoder<Tempest::CommandBuffer>::setUniforms(const RenderPipeline& p, const void* data, size_t sz) {
  setUniforms(p);
  impl->setBytes(*p.impl.handler,data,sz);
  }

void Encoder<Tempest::CommandBuffer>::setUniforms(const RenderPipeline& p,const Uniforms &ubo) {
  setUniforms(p);
  impl->setUniforms(*p.impl.handler,*ubo.desc.handler);
  }

void Encoder<Tempest::CommandBuffer>::setUniforms(const Detail::ResourcePtr<RenderPipeline> &p, const Uniforms &ubo) {
  setUniforms(p);
  impl->setUniforms(*p.impl.handler,*ubo.desc.handler);
  }

void Encoder<Tempest::CommandBuffer>::setUniforms(const Detail::ResourcePtr<RenderPipeline> &p) {
  if(state.curPipeline!=p.impl.handler) {
    impl->setPipeline(*p.impl.handler,state.vp.width,state.vp.height);
    state.curCompute  = nullptr;
    state.curPipeline = p.impl.handler;
    }
  }

void Encoder<Tempest::CommandBuffer>::setUniforms(const RenderPipeline &p) {
  if(state.curPipeline!=p.impl.handler) {
    impl->setPipeline(*p.impl.handler,state.vp.width,state.vp.height);
    state.curPipeline=p.impl.handler;
    }
  }

void Encoder<Tempest::CommandBuffer>::setUniforms(const ComputePipeline& p, const void* data, size_t sz) {
  setUniforms(p);
  impl->setBytes(*p.impl.handler,data,sz);
  }

void Encoder<Tempest::CommandBuffer>::setUniforms(const ComputePipeline& p,const Uniforms &ubo) {
  setUniforms(p);
  impl->setUniforms(*p.impl.handler,*ubo.desc.handler);
  }

void Encoder<Tempest::CommandBuffer>::setUniforms(const ComputePipeline& p) {
  if(state.curCompute!=p.impl.handler) {
    impl->setComputePipeline(*p.impl.handler);
    state.curCompute  = p.impl.handler;
    state.curPipeline = nullptr;
    }
  }

void Encoder<Tempest::CommandBuffer>::implDraw(const VideoBuffer& vbo, size_t offset, size_t size) {
  if(!vbo.impl)
    return;
  if(state.curVbo!=&vbo) {
    impl->setVbo(*vbo.impl.handler);
    state.curVbo=&vbo;
    }
  /*
   // no need to clear vbo
  if(state.curIbo!=nullptr) {
    impl->setIbo(nullptr,Detail::IndexClass::i16);
    state.curIbo=nullptr;
    }*/
  impl->draw(offset,size);
  }

void Encoder<Tempest::CommandBuffer>::implDraw(const VideoBuffer &vbo, const VideoBuffer &ibo, Detail::IndexClass index, size_t offset, size_t size) {
  if(!vbo.impl || !ibo.impl)
    return;
  if(state.curVbo!=&vbo) {
    impl->setVbo(*vbo.impl.handler);
    state.curVbo=&vbo;
    }
  if(state.curIbo!=&ibo) {
    impl->setIbo(*ibo.impl.handler,index);
    state.curIbo=&ibo;
    }
  impl->drawIndexed(offset,size,0);
  }

void Encoder<CommandBuffer>::setFramebuffer(const FrameBuffer &fbo, const RenderPass &p) {
  implEndRenderPass();

  state.vp.width  = fbo.w();
  state.vp.height = fbo.h();

  reinterpret_cast<AbstractGraphicsApi::CommandBuffer*>(impl)->beginRenderPass(fbo.impl.handler,p.impl.handler,
                                                                               state.vp.width,state.vp.height);
  curPass.fbo  = &fbo;
  curPass.pass = &p;
  }

void Encoder<CommandBuffer>::implEndRenderPass() {
  if(curPass.pass!=nullptr) {
    state.curPipeline = nullptr;
    curPass           = Pass();
    reinterpret_cast<AbstractGraphicsApi::CommandBuffer*>(impl)->endRenderPass();
    }
  }

void Encoder<CommandBuffer>::dispatch(size_t x, size_t y, size_t z) {
  impl->dispatch(x,y,z);
  }

void Encoder<CommandBuffer>::generateMipmaps(Attachment& tex) {
  uint32_t w = tex.w(), h = tex.h();
  impl->generateMipmap(*textureCast(tex).impl.handler,w,h,mipCount(w,h));
  }
