#include "vbuffer.h"

#include "vdevice.h"
#include "vallocator.h"

#include <utility>

using namespace Tempest::Detail;

VBuffer::VBuffer(VBuffer &&other) {
  std::swap(impl, other.impl);
  std::swap(alloc,other.alloc);
  std::swap(page, other.page);
  }

VBuffer::~VBuffer() {
  if(alloc!=nullptr)
    alloc->free(*this);
  }
