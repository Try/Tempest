#include "framebuffer.h"
#include <Tempest/Device>

using namespace Tempest;

FrameBufferLayout::FrameBufferLayout(Detail::DSharedPtr<AbstractGraphicsApi::FboLayout *> &&f)
  :impl(std::move(f)) {
  }

FrameBuffer::FrameBuffer(Device& dev,
                         Detail::DSharedPtr<AbstractGraphicsApi::Fbo*> &&impl,
                         FrameBufferLayout &&lay, uint32_t w, uint32_t h)
  :dev(&dev),impl(std::move(impl)),lay(std::move(lay)),mw(w),mh(h) {
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
  if(impl.handler==fbo.impl.handler)
    return true;
  if(impl.handler==nullptr || fbo.impl.handler==nullptr)
    return false;
  return impl.handler->equals(*fbo.impl.handler);
  }
