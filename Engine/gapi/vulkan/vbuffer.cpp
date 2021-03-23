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

void VBuffer::update(const void *data, size_t off, size_t count, size_t size, size_t alignedSz) {
  if(T_LIKELY(page.page->hostVisible)) {
    alloc->update(*this,data,off,count,size,alignedSz);
    return;
    }

  auto&           dx    = *alloc->device();
  Detail::VBuffer stage = dx.allocator.alloc(data,count,size,alignedSz, MemUsage::TransferSrc, BufferHeap::Upload);

  Detail::DSharedPtr<Buffer*> pstage(new Detail::VBuffer(std::move(stage)));
  Detail::DSharedPtr<Buffer*> pbuf  (this);

  auto cmd = dx.dataMgr().get();
  cmd->begin();
  cmd->hold(pbuf); // NOTE: VBuffer may be deleted, before copy is finished
  cmd->hold(pstage);
  cmd->copy(*this, off*alignedSz, *pstage.handler,0, count*alignedSz);
  cmd->end();

  dx.dataMgr().wait(); // write-after-write case
  dx.dataMgr().submit(std::move(cmd));
  }

void VBuffer::read(void *data, size_t off, size_t count, size_t sz, size_t alignedSz) {
  alloc->read(*this,data,off,count,sz,alignedSz);
  }

void VBuffer::read(void* data, size_t off, size_t sz) {
  alloc->read(*this,data,off,sz);
  }
