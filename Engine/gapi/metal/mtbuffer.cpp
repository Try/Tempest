#if defined(TEMPEST_BUILD_METAL)

#include "mtbuffer.h"
#include "mtdevice.h"

#include "utility/compiller_hints.h"
#include "gapi/graphicsmemutils.h"

using namespace Tempest::Detail;

MtBuffer::MtBuffer(MtDevice& dev, const void* data, size_t count, size_t sz, size_t alignedSz, MTL::ResourceOptions f)
  :dev(dev) {
  const MTL::ResourceOptions flg = f | MTL::HazardTrackingModeDefault;
  if(data==nullptr) {
    impl = NsPtr<MTL::Buffer>(dev.impl->newBuffer(count*alignedSz,flg));
    if(impl==nullptr)
      throw std::system_error(GraphicsErrc::OutOfVideoMemory);
    return;
    }

  if(alignedSz==sz && 0==(flg & MTL::ResourceStorageModePrivate)) {
    impl = NsPtr<MTL::Buffer>(dev.impl->newBuffer(data,count*alignedSz,flg));
    if(impl==nullptr)
      throw std::system_error(GraphicsErrc::OutOfVideoMemory);
    return;
    }

  impl = NsPtr<MTL::Buffer>(dev.impl->newBuffer(count*alignedSz,flg));
  if(impl==nullptr)
    throw std::system_error(GraphicsErrc::OutOfVideoMemory);
  update(data,0,count,sz,alignedSz);
  }

MtBuffer::~MtBuffer() {
  }

void MtBuffer::update(const void *data, size_t off, size_t count, size_t sz, size_t alignedSz) {
  if(T_LIKELY(impl->storageMode()!=MTL::StorageModePrivate)) {
    implUpdate(data,off,count,sz,alignedSz);
    return;
    }

  auto pool = NsPtr<NS::AutoreleasePool>::init();
  MTL::ResourceOptions opt   = MTL::ResourceStorageModeShared | MTL::ResourceCPUCacheModeWriteCombined;
  auto                 stage = NsPtr<MTL::Buffer>(dev.impl->newBuffer(count*alignedSz,opt));
  copyUpsample(data, stage->contents(), count, sz, alignedSz);

  auto cmd = dev.queue->commandBuffer();
  auto enc = cmd->blitCommandEncoder();
  enc->copyFromBuffer(stage.get(),0,impl.get(),0, count*alignedSz);
  enc->endEncoding();
  cmd->commit();
  // TODO: implement proper upload engine
  cmd->waitUntilCompleted();
  }

void MtBuffer::read(void *data, size_t off, size_t size) {
  auto pool = NsPtr<NS::AutoreleasePool>::init();

  MTL::ResourceOptions opt   = MTL::ResourceStorageModeShared | MTL::ResourceCPUCacheModeDefaultCache |
                               MTL::ResourceHazardTrackingModeTracked;
  auto                 stage = NsPtr<MTL::Buffer>(dev.impl->newBuffer(size,opt));
  auto cmd = dev.queue->commandBuffer();
  auto enc = cmd->blitCommandEncoder();
  enc->copyFromBuffer(impl.get(),off,stage.get(),0, size);
  enc->endEncoding();
  cmd->commit();

  cmd->waitUntilCompleted();
  std::memcpy(data,stage->contents(),size);
  }

void MtBuffer::implUpdate(const void *data, size_t off, size_t count, size_t sz, size_t alignedSz) {
  auto ptr = reinterpret_cast<uint8_t*>(impl->contents());

  copyUpsample(data, ptr+off, count, sz, alignedSz);
  if(impl->storageMode()!=MTL::StorageModeManaged)
    return;

  impl->didModifyRange(NS::Range(off*alignedSz,count*alignedSz));
  }

#endif
