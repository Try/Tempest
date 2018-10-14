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
  impl.handler->end();
  }

void CommandBuffer::beginRenderPass(const FrameBuffer &fbo, const RenderPass &p,uint32_t width,uint32_t height) {
  impl.handler->beginRenderPass(fbo.impl.handler,p.impl.handler,width,height);
  }

void CommandBuffer::endRenderPass() {
  impl.handler->endRenderPass();
  }

void CommandBuffer::setPipeline(RenderPipeline &p) {
  impl.handler->setPipeline(*p.impl.handler);
  }

void CommandBuffer::draw(size_t size) {
  impl.handler->draw(size);
  }

void CommandBuffer::setVbo(const VideoBuffer& vbo) {
  impl.handler->setVbo(*vbo.impl.handler);
  }


