#include "encoder.h"

#include <Tempest/FrameBuffer>
#include <Tempest/RenderPass>
#include <Tempest/Texture2d>

using namespace Tempest;

Encoder<Tempest::CommandBuffer>::Encoder(CommandBuffer* ow)
  :owner(ow),impl(ow->impl.handler){
  state.vp.width  = ow->vpWidth;
  state.vp.height = ow->vpHeight;

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
  impl->setUniforms(*p.impl.handler,*ubo.desc.handler,0,nullptr);
  }

void Encoder<Tempest::CommandBuffer>::setUniforms(const Tempest::RenderPipeline& p,const Uniforms &ubo,size_t offc,const uint32_t* offv) {
  setUniforms(p);
  impl->setUniforms(*p.impl.handler,*ubo.desc.handler,offc,offv);
  }

void Encoder<Tempest::CommandBuffer>::setUniforms(const Detail::ResourcePtr<RenderPipeline> &p, const Uniforms &ubo) {
  setUniforms(p);
  impl->setUniforms(*p.impl.handler,*ubo.desc.handler,0,nullptr);
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
  if(state.curIbo!=nullptr) {
    impl->setIbo(nullptr,Detail::IndexClass::i16);
    state.curIbo=nullptr;
    }
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
    impl->setIbo(ibo.impl.handler,index);
    state.curIbo=&ibo;
    }
  impl->drawIndexed(offset,size,0);
  }


Encoder<PrimaryCommandBuffer>::Encoder(PrimaryCommandBuffer *ow)
  :Encoder<CommandBuffer>(ow->impl.handler) {
  }

Encoder<PrimaryCommandBuffer>::Encoder(Encoder<PrimaryCommandBuffer> &&e)
  :Encoder<CommandBuffer>(std::move(e)), curPass(std::move(e.curPass)), resState(std::move(e.resState)) {
  }

Encoder<PrimaryCommandBuffer> &Encoder<PrimaryCommandBuffer>::operator =(Encoder<PrimaryCommandBuffer> &&e) {
  Encoder<CommandBuffer>::operator=(std::move(e));
  std::swap(curPass, e.curPass);
  std::swap(resState,e.resState);
  return *this;
  }

Encoder<Tempest::PrimaryCommandBuffer>::~Encoder() noexcept(false) {
  if(impl==nullptr)
    return;
  implEndRenderPass();
  impl->end();
  impl=nullptr;
  }

void Encoder<PrimaryCommandBuffer>::setPass(const FrameBuffer &fbo, const RenderPass &p) {
  setPass(fbo,p,fbo.w(),fbo.h());
  }

void Encoder<PrimaryCommandBuffer>::setPass(const FrameBuffer &fbo, const RenderPass &p, int width, int height) {
  setPass(fbo,p,uint32_t(width),uint32_t(height));
  }

void Encoder<PrimaryCommandBuffer>::setPass(const FrameBuffer &fbo, const RenderPass &p, uint32_t width, uint32_t height) {
  implEndRenderPass();

  state.vp.width     = width;
  state.vp.height    = height;
  //setViewport(0,0,int(width),int(height));

  impl->beginRenderPass(fbo.impl.handler,p.impl.handler,width,height);
  curPass.fbo  = &fbo;
  curPass.pass = &p;
  }

void Encoder<PrimaryCommandBuffer>::implEndRenderPass() {
  if(curPass.pass!=nullptr) {
    state.curPipeline = nullptr;
    curPass           = Pass();
    impl->endRenderPass();
    }

  for(auto& i:resState)
    if(i.pending) {
      impl->changeLayout(*i.res,i.frm,i.lay,i.next);
      i.lay     = i.next;
      i.pending = false;
      }
  }

Encoder<PrimaryCommandBuffer>::ResState *Encoder<PrimaryCommandBuffer>::findState(AbstractGraphicsApi::Texture *handler) {
  for(auto& i:resState)
    if(i.res==handler) {
      return &i;
      }
  ResState st;
  st.res = handler;
  st.lay = TextureLayout::Undefined;
  resState.push_back(st);
  return &resState.back();
  }

void Encoder<Tempest::PrimaryCommandBuffer>::setLayout(Texture2d &t, TextureLayout dest) {
  if(t.impl.handler==nullptr)
    return;

  ResState* st = findState(t.impl.handler);
  st->frm      = t.frm;
  st->next     = dest;
  st->pending  = true;
  }

void Encoder<PrimaryCommandBuffer>::exec(const CommandBuffer &buf) {
  if(buf.impl.handler==nullptr)
    return;
  impl->exec(*buf.impl.handler);
  }
