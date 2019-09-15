#include "framebuffer.h"
#include <Tempest/Device>

using namespace Tempest;

FrameBuffer::FrameBuffer(Device &dev, Detail::DSharedPtr<AbstractGraphicsApi::Fbo*> &&impl, uint32_t w, uint32_t h)
  :dev(&dev),impl(std::move(impl)),mw(w),mh(h) {
  }

FrameBuffer::~FrameBuffer() {
  }
