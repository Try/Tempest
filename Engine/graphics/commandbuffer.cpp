#include "commandbuffer.h"

#include <Tempest/Device>
#include <Tempest/RenderPipeline>

using namespace Tempest;

CommandBuffer::CommandBuffer(Device &dev, AbstractGraphicsApi::CommandBuffer *impl,uint32_t vpWidth,uint32_t vpHeight)
  :dev(&dev),impl(impl) {
  vp.width  = vpWidth;
  vp.height = vpHeight;
  }

CommandBuffer::~CommandBuffer() {
  dev->destroy(*this);
  }

void CommandBuffer::begin() {
  impl.handler->begin();
  if(vp.width>0 && vp.height>0)
    setViewport(0,0,int(vp.width),int(vp.height));
  }

void CommandBuffer::end() {
  implEndRenderPass();
  curVbo      = nullptr;
  curIbo      = nullptr;
  curPipeline = nullptr;
  impl.handler->end();
  }

void CommandBuffer::setViewport(int x, int y, int w, int h) {
  impl.handler->setViewport(Rect(x,y,w,h));
  }

void CommandBuffer::setViewport(const Rect &vp) {
  impl.handler->setViewport(vp);
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
    impl.handler->setPipeline(*p.impl.handler,vp.width,vp.height);
    curPipeline=p.impl.handler;
    }
  }

void CommandBuffer::setUniforms(const Detail::ResourcePtr<RenderPipeline> &p) {
  if(curPipeline!=p.impl.handler) {
    impl.handler->setPipeline(*p.impl.handler,vp.width,vp.height);
    curPipeline=p.impl.handler;
    }
  }

void CommandBuffer::exchangeLayout(Texture2d &t, TextureLayout src, TextureLayout dest) {
  implEndRenderPass();

  if(t.impl.handler)
    impl.handler->changeLayout(*t.impl.handler,src,dest);
  }

void CommandBuffer::barrier(Texture2d &t, Stage in, Stage out) {
  implEndRenderPass();

  if(t.impl.handler)
    impl.handler->barrier(*t.impl.handler,in,out);
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


void PrimaryCommandBuffer::setPass(const FrameBuffer &fbo, const RenderPass &p) {
  setPass(fbo,p,fbo.w(),fbo.h());
  }

void PrimaryCommandBuffer::setPass(const FrameBuffer &fbo, const RenderPass &p, int width, int height) {
  setPass(fbo,p,uint32_t(width),uint32_t(height));
  }

void PrimaryCommandBuffer::setPass(const FrameBuffer &fbo, const RenderPass &p, uint32_t width, uint32_t height) {
  implEndRenderPass();
  impl.handler->beginRenderPass(fbo.impl.handler,p.impl.handler,width,height);
  curPass.fbo  = &fbo;
  curPass.pass = &p;
  curPass.mode = Prime;

  vp.width     = width;
  vp.height    = height;
  setViewport(0,0,int(width),int(height));
  }

void PrimaryCommandBuffer::implEndRenderPass() {
  if(curPass.mode==Idle)
    return;
  curPipeline = nullptr;
  curPass     = Pass();
  impl.handler->endRenderPass();
  }

void PrimaryCommandBuffer::exec(const FrameBuffer &fbo, const RenderPass &p, const CommandBuffer &buf) {
  exec(fbo,p,fbo.w(),fbo.h(),buf);
  }

void PrimaryCommandBuffer::exec(const FrameBuffer& fbo, const RenderPass& p,uint32_t width, uint32_t height,const CommandBuffer &buf) {
  if(buf.impl.handler==nullptr)
    return;

  if(curPass.fbo!=&fbo || curPass.pass!=&p || vp.width!=width || vp.height!=height || curPass.mode!=Second) {
    implEndRenderPass();
    impl.handler->beginSecondaryPass(fbo.impl.handler,p.impl.handler,width,height);

    curPass.fbo  = &fbo;
    curPass.pass = &p;
    vp.width     = width;
    vp.height    = height;
    curPass.mode = Second;
    }

  impl.handler->exec(*buf.impl.handler);
  }
