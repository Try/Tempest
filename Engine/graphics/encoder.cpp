#include "encoder.h"

#include <Tempest/Attachment>
#include <Tempest/FrameBuffer>
#include <Tempest/RenderPass>
#include <Tempest/Texture2d>

using namespace Tempest;

Encoder<Tempest::CommandBuffer>::Encoder(CommandBuffer* ow,int w,int h)
  :owner(ow),impl(ow->impl.handler){
  state.vp.width  = uint32_t(w);
  state.vp.height = uint32_t(h);

  impl->begin();
  if(state.vp.width>0 && state.vp.height>0)
    setViewport(0,0,int(state.vp.width),int(state.vp.height));
  }

Encoder<Tempest::CommandBuffer>::Encoder(AbstractGraphicsApi::CommandBuffer* impl)
  :owner(nullptr),impl(impl){
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

void Encoder<Tempest::CommandBuffer>::setUniforms(const Tempest::RenderPipeline& p,const Uniforms &ubo) {
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
    state.curPipeline=p.impl.handler;
    }
  }
void Encoder<Tempest::CommandBuffer>::setUniforms(const RenderPipeline &p) {
  if(state.curPipeline!=p.impl.handler) {
    impl->setPipeline(*p.impl.handler,state.vp.width,state.vp.height);
    state.curPipeline=p.impl.handler;
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


Encoder<PrimaryCommandBuffer>::Encoder(PrimaryCommandBuffer *ow)
  :Encoder<CommandBuffer>(ow->impl.handler) {
  }

Encoder<PrimaryCommandBuffer>::Encoder(Encoder<PrimaryCommandBuffer> &&e)
  :Encoder<CommandBuffer>(std::move(e)), curPass(std::move(e.curPass)) {
  }

Encoder<PrimaryCommandBuffer> &Encoder<PrimaryCommandBuffer>::operator =(Encoder<PrimaryCommandBuffer> &&e) {
  Encoder<CommandBuffer>::operator=(std::move(e));
  std::swap(curPass, e.curPass);
  return *this;
  }

Encoder<Tempest::PrimaryCommandBuffer>::~Encoder() noexcept(false) {
  if(impl==nullptr)
    return;
  implEndRenderPass();
  impl->end();
  impl=nullptr;
  }

void Encoder<PrimaryCommandBuffer>::setFramebuffer(const FrameBuffer &fbo, const RenderPass &p) {
  implEndRenderPass();

  state.vp.width  = fbo.w();
  state.vp.height = fbo.h();

  reinterpret_cast<AbstractGraphicsApi::CommandBuffer*>(impl)->beginRenderPass(fbo.impl.handler,p.impl.handler,
                                                                               state.vp.width,state.vp.height);
  curPass.fbo  = &fbo;
  curPass.pass = &p;
  }

void Encoder<PrimaryCommandBuffer>::implEndRenderPass() {
  if(curPass.pass!=nullptr) {
    state.curPipeline = nullptr;
    curPass           = Pass();
    reinterpret_cast<AbstractGraphicsApi::CommandBuffer*>(impl)->endRenderPass();
    }
  }

void Encoder<PrimaryCommandBuffer>::exec(const CommandBuffer &buf) {
  if(buf.impl.handler==nullptr)
    return;
  reinterpret_cast<AbstractGraphicsApi::CommandBuffer*>(impl)->exec(*buf.impl.handler);
  //FIXME: vulkan backend issue - implementation not preserving state across command-chunks
  state.curPipeline = nullptr;
  state.curVbo      = nullptr;
  state.curIbo      = nullptr;
  }
