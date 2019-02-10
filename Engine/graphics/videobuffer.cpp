#include "videobuffer.h"

#include <Tempest/Device>
#include <algorithm>

using namespace Tempest;

VideoBuffer::VideoBuffer(Device &dev, AbstractGraphicsApi::Buffer *impl, size_t size)
  :dev(&dev),impl(impl),sz(size) {
  }

VideoBuffer::VideoBuffer(VideoBuffer &&other)
  :dev(other.dev),impl(std::move(other.impl)),sz(other.sz){
  other.dev=nullptr;
  }

VideoBuffer::~VideoBuffer(){
  if(dev)
    dev->destroy(*this);
  }

VideoBuffer &VideoBuffer::operator=(VideoBuffer &&other) {
  std::swap(dev,other.dev);
  impl = std::move(other.impl);
  sz   = other.sz;
  return *this;
  }

void VideoBuffer::update(const void *data, size_t offset, size_t size) {
  if(sz==0)
    return;
  if(offset+size>sz)
    throw std::logic_error("invalid VideoBuffer update range");
  impl.handler->update(data,offset,size);
  }
