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

void CommandBuffer::end() {
  curVbo=nullptr;
  curPipeline=nullptr;
  impl.handler->end();
  }

void CommandBuffer::beginRenderPass(const FrameBuffer &fbo, const RenderPass &p,uint32_t width,uint32_t height) {
  impl.handler->beginRenderPass(fbo.impl.handler,p.impl.handler,width,height);
  }

void CommandBuffer::endRenderPass() {
  curPipeline=nullptr;
  impl.handler->endRenderPass();
  }

void CommandBuffer::setUniforms(const Tempest::RenderPipeline& p,const Uniforms &ubo) {
  if(curPipeline!=&p) {
    impl.handler->setPipeline(*p.impl.handler);
    curPipeline=&p;
    }
  impl.handler->setUniforms(*p.impl.handler,*ubo.desc.handler);
  }

void CommandBuffer::exec(const CommandBuffer &buf) {
  impl.handler->exec(*buf.impl.handler);
  }

void CommandBuffer::implDraw(const VideoBuffer& vbo,size_t size) {
  if(curVbo!=&vbo && vbo.impl) {
    impl.handler->setVbo(*vbo.impl.handler);
    curVbo=&vbo;
    }
  impl.handler->draw(size);
  }


