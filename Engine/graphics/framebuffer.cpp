#include "framebuffer.h"
#include <Tempest/Device>

using namespace Tempest;

FrameBuffer::FrameBuffer(Device &dev, AbstractGraphicsApi::Fbo *impl)
  :dev(&dev),impl(impl) {
  }

FrameBuffer::~FrameBuffer() {
  dev->destroy(*this);
  }
