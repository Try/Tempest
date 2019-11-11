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

bool FrameBufferLayout::operator != (const FrameBufferLayout &fbo) const {
  return !(*this==fbo);
  }

bool FrameBufferLayout::operator == (const FrameBufferLayout &fbo) const {
  if(mw!=fbo.mw || mh!=fbo.mh)
    return false;
  if(impl.handler==fbo.impl.handler)
    return true;
  if(impl.handler==nullptr || fbo.impl.handler==nullptr)
    return false;
  return impl.handler->equals(*fbo.impl.handler);
  }
