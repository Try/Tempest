#include "framebuffer.h"
#include <Tempest/Device>

using namespace Tempest;

FrameBufferLayout::FrameBufferLayout(Detail::DSharedPtr<AbstractGraphicsApi::FboLayout *> &&f,uint32_t w,uint32_t h)
  :impl(std::move(f)), mw(w), mh(h) {
  }

FrameBuffer::FrameBuffer(Device &dev,
                         Detail::DSharedPtr<AbstractGraphicsApi::Fbo*> &&impl,
                         FrameBufferLayout &&lay)
  :dev(&dev),impl(std::move(impl)),lay(std::move(lay)) {
  }

FrameBuffer::~FrameBuffer() {
  }

const FrameBufferLayout& FrameBuffer::layout() const {
  return lay;
  }
