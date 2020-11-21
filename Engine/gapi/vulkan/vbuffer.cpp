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

VBuffer& VBuffer::operator=(VBuffer&& other) {
  std::swap(impl, other.impl);
  std::swap(alloc,other.alloc);
  std::swap(page, other.page);
  return *this;
  }

void VBuffer::update(const void *data, size_t off, size_t count, size_t sz, size_t alignedSz) {
  if(alloc!=nullptr)
    alloc->update(*this,data,off,count,sz,alignedSz);
  }

void VBuffer::read(void *data, size_t off, size_t count, size_t sz, size_t alignedSz) {
  if(alloc!=nullptr)
    alloc->read(*this,data,off,count,sz,alignedSz);
  }

void VBuffer::read(void* data, size_t off, size_t sz) {
  if(alloc!=nullptr)
    alloc->read(*this,data,off,sz);
  }
