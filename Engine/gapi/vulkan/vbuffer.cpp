#if defined(TEMPEST_BUILD_VULKAN)

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
  auto& dx = *alloc->device();

  if(T_LIKELY(page.page->hostVisible)) {
    dx.dataMgr().waitFor(this); // write-after-write case
    alloc->update(*this,data,off,count,size,alignedSz);
    return;
    }

  if(size==alignedSz && off%4==0 && (count*alignedSz)%4==0) {
    Detail::DSharedPtr<Buffer*> pBuf  (this);

    auto cmd = dx.dataMgr().get();
    cmd->begin();
    cmd->hold(pBuf); // NOTE: VBuffer may be deleted, before copy is finished
    cmd->copy(*this, off*alignedSz, data, count*alignedSz);
    cmd->end();

    dx.dataMgr().waitFor(this); // write-after-write case
    dx.dataMgr().submit(std::move(cmd));
    return;
    }

  auto  stage = dx.dataMgr().allocStagingMemory(data,count,size,alignedSz,MemUsage::TransferSrc,BufferHeap::Upload);

  Detail::DSharedPtr<Buffer*> pStage(new Detail::VBuffer(std::move(stage)));
  Detail::DSharedPtr<Buffer*> pBuf  (this);

  auto cmd = dx.dataMgr().get();
  cmd->begin();
  cmd->hold(pBuf); // NOTE: VBuffer may be deleted, before copy is finished
  cmd->hold(pStage);
  cmd->copy(*this, off*alignedSz, *pStage.handler,0, count*alignedSz);
  cmd->end();

  dx.dataMgr().waitFor(this); // write-after-write case
  dx.dataMgr().submit(std::move(cmd));
  }

void VBuffer::read(void* out, size_t off, size_t size) {
  auto& dx = *alloc->device();

  if(T_LIKELY(page.page->hostVisible)) {
    dx.dataMgr().waitFor(this); // Buffer::update can be in flight
    alloc->read(*this,out,off,size);
    return;
    }

  auto  stage = dx.dataMgr().allocStagingMemory(nullptr,size,1,1,MemUsage::TransferDst,BufferHeap::Readback);

  auto cmd = dx.dataMgr().get();
  cmd->begin();
  cmd->copy(stage,0, *this,off,size);
  cmd->end();

  dx.dataMgr().waitFor(this); // Buffer::update can be in flight
  dx.dataMgr().submitAndWait(std::move(cmd));

  stage.read(out,0,size);
  }

#endif
