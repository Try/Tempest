#include "framebuffer.h"
#include <Tempest/Device>

using namespace Tempest;

FrameBuffer::FrameBuffer(Device &dev, AbstractGraphicsApi::Fbo *impl, uint32_t w, uint32_t h)
  :dev(&dev),impl(impl),mw(w),mh(h) {
  }

FrameBuffer::~FrameBuffer() {
  dev->destroy(*this);
  }
