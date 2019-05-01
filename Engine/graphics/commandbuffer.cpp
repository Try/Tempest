#include "commandbuffer.h"

#include <Tempest/Device>
#include <Tempest/RenderPipeline>

using namespace Tempest;

CommandBuffer::CommandBuffer(Device &dev, AbstractGraphicsApi::CommandBuffer *impl)
  :dev(&dev),impl(impl) {
  }

CommandBuffer::~CommandBuffer() {
  dev->destroy(*this);
  }

void CommandBuffer::begin() {
  impl.handler->begin();
  }

void CommandBuffer::begin(const RenderPass &p) {
  impl.handler->begin(p.impl.handler);
  }

void CommandBuffer::end() {
  if(curPass.active)
    implEndRenderPass();
  curPass     = Pass();
  curVbo      = nullptr;
  curIbo      = nullptr;
  curPipeline = nullptr;
  impl.handler->end();
  }

void CommandBuffer::setPass(const RenderPass &p) {
  if(curPass.active)
    impl.handler->next(p.impl.handler);
  }

void CommandBuffer::implEndRenderPass() {
  curPipeline = nullptr;
  curPass     = Pass();
  impl.handler->endRenderPass();
  }

void CommandBuffer::setPass(const FrameBuffer &fbo, const RenderPass &p) {
  setPass(fbo,p,fbo.w(),fbo.h());
  }

void CommandBuffer::setPass(const FrameBuffer &fbo, const RenderPass &p, int width, int height) {
  setPass(fbo,p,uint32_t(width),uint32_t(height));
  }

void CommandBuffer::setPass(const FrameBuffer &fbo, const RenderPass &p, uint32_t width, uint32_t height) {
  if(curPass.active)
    implEndRenderPass();
  impl.handler->beginRenderPass(fbo.impl.handler,p.impl.handler,width,height);
  curPass.fbo    = &fbo;
  curPass.pass   = &p;
  curPass.width  = width;
  curPass.height = height;
  curPass.active = true;
  }

void CommandBuffer::setSecondaryPass(const FrameBuffer &fbo, const RenderPass &p) {
  setSecondaryPass(fbo,p,fbo.w(),fbo.h());
  }

void CommandBuffer::setSecondaryPass(const FrameBuffer &fbo, const RenderPass &p, uint32_t width, uint32_t height) {
  if(curPass.active)
    implEndRenderPass();
  impl.handler->beginSecondaryPass(fbo.impl.handler,p.impl.handler,width,height);
  curPass.fbo    = &fbo;
  curPass.pass   = &p;
  curPass.width  = width;
  curPass.height = height;
  curPass.active = true;
  }

void CommandBuffer::setUniforms(const Tempest::RenderPipeline& p,const Uniforms &ubo) {
  setUniforms(p);
  impl.handler->setUniforms(*p.impl.handler,*ubo.desc.handler,0,nullptr);
  }

void CommandBuffer::setUniforms(const Tempest::RenderPipeline& p,const Uniforms &ubo,size_t offc,const uint32_t* offv) {
  setUniforms(p);
  impl.handler->setUniforms(*p.impl.handler,*ubo.desc.handler,offc,offv);
  }

void CommandBuffer::setUniforms(const Detail::ResourcePtr<RenderPipeline> &p, const Uniforms &ubo) {
  setUniforms(p);
  impl.handler->setUniforms(*p.impl.handler,*ubo.desc.handler,0,nullptr);
  }

void CommandBuffer::setUniforms(const RenderPipeline &p) {
  if(curPipeline!=p.impl.handler) {
    impl.handler->setPipeline(*p.impl.handler);
    curPipeline=p.impl.handler;
    }
  }

void CommandBuffer::setUniforms(const Detail::ResourcePtr<RenderPipeline> &p) {
  if(curPipeline!=p.impl.handler) {
    impl.handler->setPipeline(*p.impl.handler);
    curPipeline=p.impl.handler;
    }
  }

void CommandBuffer::setViewport(int x, int y, int w, int h) {
  impl.handler->setViewport(Rect(x,y,w,h));
  }

void CommandBuffer::setViewport(const Rect &vp) {
  impl.handler->setViewport(vp);
  }

void CommandBuffer::exec(const CommandBuffer &buf) {
  if(buf.impl.handler)
    impl.handler->exec(*buf.impl.handler);
  }

void CommandBuffer::changeLayout(Texture2d &t, TextureLayout prev, TextureLayout next) {
  if(t.impl.handler)
    impl.handler->changeLayout(*t.impl.handler,prev,next);
  }

void CommandBuffer::implDraw(const VideoBuffer& vbo, size_t offset, size_t size) {
  if(!vbo.impl)
    return;
  if(curVbo!=&vbo) {
    impl.handler->setVbo(*vbo.impl.handler);
    curVbo=&vbo;
    }
  if(curIbo!=nullptr) {
    impl.handler->setIbo(nullptr,Detail::IndexClass::i16);
    curIbo=nullptr;
    }
  impl.handler->draw(offset,size);
  }

void CommandBuffer::implDraw(const VideoBuffer &vbo, const VideoBuffer &ibo, Detail::IndexClass index, size_t offset, size_t size) {
  if(!vbo.impl || !ibo.impl)
    return;
  if(curVbo!=&vbo) {
    impl.handler->setVbo(*vbo.impl.handler);
    curVbo=&vbo;
    }
  if(curIbo!=&ibo) {
    impl.handler->setIbo(ibo.impl.handler,index);
    curIbo=&ibo;
    }
  impl.handler->drawIndexed(offset,size,0);
  }
