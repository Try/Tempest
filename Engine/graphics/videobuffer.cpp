#include "videobuffer.h"

#include <Tempest/Device>
#include <Tempest/Except>
#include <algorithm>

using namespace Tempest;

VideoBuffer::VideoBuffer(AbstractGraphicsApi::PBuffer&& impl, size_t size)
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

void VideoBuffer::update(const void *data, size_t offset, size_t count, size_t size, size_t alignedSz) {
  if(count==0)
    return;
  if((offset+count)*alignedSz>sz)
    throw std::system_error(Tempest::GraphicsErrc::InvalidBufferUpdate);
  impl.handler->update(data,offset,count,size,alignedSz);
  }
