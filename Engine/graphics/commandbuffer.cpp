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

void CommandBuffer::clear(Frame &fr, float r, float g, float b, float a) {
  if(fr.img==nullptr)
    return;
  impl.handler->clear(*fr.img,r,g,b,a);
  }

void CommandBuffer::begin() {
  impl.handler->begin();
  }

void CommandBuffer::begin(const RenderPass &p) {
  impl.handler->begin(p.impl.handler);
  }

void CommandBuffer::end() {
  curVbo=nullptr;
  curIbo=nullptr;
  curPipeline=nullptr;
  impl.handler->end();
  }

void CommandBuffer::beginRenderPass(const FrameBuffer &fbo, const RenderPass &p) {
  beginRenderPass(fbo,p,fbo.w(),fbo.h());
  }

void CommandBuffer::beginRenderPass(const FrameBuffer &fbo, const RenderPass &p, int width, int height) {
  beginRenderPass(fbo,p,uint32_t(width),uint32_t(height));
  }

void CommandBuffer::beginRenderPass(const FrameBuffer &fbo, const RenderPass &p,uint32_t width,uint32_t height) {
  impl.handler->beginRenderPass(fbo.impl.handler,p.impl.handler,width,height);
  }

void CommandBuffer::beginSecondaryPasses(const FrameBuffer &fbo, const RenderPass &p) {
  impl.handler->beginSecondaryPass(fbo.impl.handler,p.impl.handler,fbo.w(),fbo.h());
  }

void CommandBuffer::beginSecondaryPasses(const FrameBuffer &fbo, const RenderPass &p, uint32_t width, uint32_t height) {
  impl.handler->beginSecondaryPass(fbo.impl.handler,p.impl.handler,width,height);
  }

void CommandBuffer::endRenderPass() {
  curPipeline=nullptr;
  impl.handler->endRenderPass();
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

void CommandBuffer::exec(const CommandBuffer &buf) {
  impl.handler->exec(*buf.impl.handler);
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
