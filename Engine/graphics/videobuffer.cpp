#include "videobuffer.h"

#include <Tempest/Device>
#include <algorithm>

using namespace Tempest;

VideoBuffer::VideoBuffer(Device &, AbstractGraphicsApi::PBuffer&& impl, size_t size)
  :impl(std::move(impl)),sz(size) {
  }

VideoBuffer::VideoBuffer(VideoBuffer &&other)
  :impl(std::move(other.impl)),sz(other.sz){
  }

VideoBuffer::~VideoBuffer(){
  }

VideoBuffer &VideoBuffer::operator=(VideoBuffer &&other) {
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
