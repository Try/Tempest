#include "videobuffer.h"

#include <Tempest/Device>
#include <Tempest/Except>
#include <algorithm>

using namespace Tempest;
using namespace Tempest::Detail;

VideoBuffer::VideoBuffer(AbstractGraphicsApi::PBuffer&& impl, size_t size)
  :impl(std::move(impl)),sz(size) {
  }

VideoBuffer::VideoBuffer(VideoBuffer &&other)
  :impl(std::move(other.impl)),sz(other.sz) {
  other.sz = 0;
  }

VideoBuffer::~VideoBuffer(){
  }

VideoBuffer &VideoBuffer::operator=(VideoBuffer &&other) {
  std::swap(impl,other.impl);
  std::swap(sz,  other.sz);
  return *this;
  }

void VideoBuffer::update(const void* data, size_t offset, size_t size) {
  if(size==0)
    return;
  if(offset+size>sz)
    throw std::system_error(Tempest::GraphicsErrc::InvalidBufferUpdate);
  impl.handler->update(data,offset,size);
  }
