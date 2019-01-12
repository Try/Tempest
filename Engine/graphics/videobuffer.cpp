#include "videobuffer.h"

#include <Tempest/Device>
#include <algorithm>

using namespace Tempest;

VideoBuffer::VideoBuffer(Device &dev, AbstractGraphicsApi::Buffer *impl)
  :dev(&dev),impl(impl) {
  }

VideoBuffer::VideoBuffer(VideoBuffer &&other)
  :dev(other.dev),impl(std::move(other.impl)){
  other.dev=nullptr;
  }

VideoBuffer::~VideoBuffer(){
  if(dev)
    dev->destroy(*this);
  }

VideoBuffer &VideoBuffer::operator=(VideoBuffer &&other) {
  std::swap(dev,other.dev);
  impl = std::move(other.impl);
  }

