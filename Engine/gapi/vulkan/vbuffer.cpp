#if defined(TEMPEST_BUILD_VULKAN)

#include "vbuffer.h"

#include "vdevice.h"
#include "vallocator.h"

#include <utility>

using namespace Tempest::Detail;

VBuffer::VBuffer(VBuffer &&other) {
  *this = std::move(other);
  }

VBuffer::~VBuffer() {
  if(impl!=VK_NULL_HANDLE)
    vkDestroyBuffer(alloc->device()->device.impl,impl,nullptr);
  if(alloc!=nullptr)
    alloc->free(page);
  }

VBuffer& VBuffer::operator=(VBuffer&& other) {
  std::swap(impl,      other.impl);
  std::swap(nonUniqId, other.nonUniqId);
  std::swap(alloc,     other.alloc);
  std::swap(page,      other.page);
  return *this;
  }

void VBuffer::update(const void* data, size_t off, size_t size) {
  auto& dx = *alloc->device();

  if(T_LIKELY(page.page->hostVisible)) {
    dx.dataMgr().waitFor(this); // write-after-write case
    alloc->update(*this,data,off,size);
    return;
    }

  if(off%4==0 && size%4==0) {
    Detail::DSharedPtr<Buffer*> pBuf(this);

    auto cmd = dx.dataMgr().get();
    cmd->begin(true);
    cmd->hold(pBuf); // NOTE: VBuffer may be deleted, before copy is finished
    cmd->copy(*this, off, data, size);
    cmd->end();

    dx.dataMgr().waitFor(this); // write-after-write case
    dx.dataMgr().submit(std::move(cmd));
    return;
    }

  auto stage = dx.dataMgr().allocStagingMemory(data,size,MemUsage::TransferSrc,BufferHeap::Upload);

  Detail::DSharedPtr<Buffer*> pStage(new Detail::VBuffer(std::move(stage)));
  Detail::DSharedPtr<Buffer*> pBuf  (this);

  auto cmd = dx.dataMgr().get();
  cmd->begin(true);
  cmd->hold(pBuf); // NOTE: VBuffer may be deleted, before copy is finished
  cmd->hold(pStage);
  cmd->copy(*this, off, *pStage.handler, 0, size);
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

  auto  stage = dx.dataMgr().allocStagingMemory(nullptr,size,MemUsage::TransferDst,BufferHeap::Readback);

  auto cmd = dx.dataMgr().get();
  cmd->begin(true);
  cmd->copy(stage,0, *this,off,size);
  cmd->end();

  dx.dataMgr().waitFor(this); // Buffer::update can be in flight
  dx.dataMgr().submitAndWait(std::move(cmd));

  stage.read(out,0,size);
  }

void VBuffer::fill(uint32_t data, size_t off, size_t size) {
  auto& dx = *alloc->device();

  if(T_LIKELY(page.page->hostVisible)) {
    dx.dataMgr().waitFor(this); // write-after-write case
    alloc->fill(*this,data,off,size);
    return;
    }

  if(off%4==0 && size%4==0) {
    Detail::DSharedPtr<Buffer*> pBuf(this);

    auto cmd = dx.dataMgr().get();
    cmd->begin(true);
    cmd->hold(pBuf); // NOTE: VBuffer may be deleted, before copy is finished
    cmd->fill(*this, off, data, size);
    cmd->end();

    dx.dataMgr().waitFor(this); // write-after-write case
    dx.dataMgr().submit(std::move(cmd));
    return;
    }
  assert(false);
  }

bool VBuffer::isHostVisible() const {
  return page.page->hostVisible;
  }

VkDeviceAddress VBuffer::toDeviceAddress(VDevice& owner) const {
  VkBufferDeviceAddressInfo bufferDeviceAddressInfo = {};
  bufferDeviceAddressInfo.sType  = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
  bufferDeviceAddressInfo.buffer = impl;
  return owner.vkGetBufferDeviceAddress(owner.device.impl, &bufferDeviceAddressInfo);
  }

#endif
