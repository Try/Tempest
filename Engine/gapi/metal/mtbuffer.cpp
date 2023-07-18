#if defined(TEMPEST_BUILD_METAL)

#include "mtbuffer.h"
#include "mtdevice.h"

#include "utility/compiller_hints.h"
#include "gapi/graphicsmemutils.h"

using namespace Tempest::Detail;

MtBuffer::MtBuffer() {
  }

MtBuffer::MtBuffer(MtDevice& dev, const void* data, size_t size, MTL::ResourceOptions f)
  :dev(&dev), size(size) {
  const size_t roundSize = ((size+64-1)/64)*64; // for uniforms/ssbo structures in msl

  const MTL::ResourceOptions flg = f | MTL::HazardTrackingModeDefault;
  if(data==nullptr) {
    impl = NsPtr<MTL::Buffer>(dev.impl->newBuffer(roundSize,flg));
    if(impl==nullptr)
      throw std::system_error(GraphicsErrc::OutOfVideoMemory);
    return;
    }

  if(0==(flg & MTL::ResourceStorageModePrivate)) {
    impl = NsPtr<MTL::Buffer>(dev.impl->newBuffer(data,roundSize,flg));
    if(impl==nullptr)
      throw std::system_error(GraphicsErrc::OutOfVideoMemory);
    return;
    }

  impl = NsPtr<MTL::Buffer>(dev.impl->newBuffer(roundSize,flg));
  if(impl==nullptr)
    throw std::system_error(GraphicsErrc::OutOfVideoMemory);
  update(data,0,size);
  }

MtBuffer::~MtBuffer() {
  }

MtBuffer& MtBuffer::operator =(MtBuffer&& other) {
  assert(other.counter.load()==0); // internal use only
  assert(this->counter.load()==0);
  std::swap(dev,  other.dev);
  std::swap(size, other.size);
  std::swap(impl, other.impl);
  return *this;
  }

void MtBuffer::update(const void *data, size_t off, size_t size) {
  if(T_LIKELY(impl->storageMode()!=MTL::StorageModePrivate)) {
    implUpdate(data,off,size);
    return;
    }

  auto pool = NsPtr<NS::AutoreleasePool>::init();
  MTL::ResourceOptions opt   = MTL::ResourceStorageModeShared | MTL::ResourceCPUCacheModeWriteCombined;
  auto                 stage = NsPtr<MTL::Buffer>(dev->impl->newBuffer(size,opt));
  std::memcpy(stage->contents(), data, size);

  auto cmd = dev->queue->commandBuffer();
  auto enc = cmd->blitCommandEncoder();
  enc->copyFromBuffer(stage.get(), 0,impl.get(), 0,size);
  enc->endEncoding();
  cmd->commit();
  // TODO: implement proper upload engine
  cmd->waitUntilCompleted();
  }

void MtBuffer::read(void *data, size_t off, size_t size) {
  auto pool = NsPtr<NS::AutoreleasePool>::init();

  MTL::ResourceOptions opt   = MTL::ResourceStorageModeShared | MTL::ResourceCPUCacheModeDefaultCache |
                               MTL::ResourceHazardTrackingModeTracked;
  auto                 stage = NsPtr<MTL::Buffer>(dev->impl->newBuffer(size,opt));
  auto cmd = dev->queue->commandBuffer();
  auto enc = cmd->blitCommandEncoder();
  enc->copyFromBuffer(impl.get(),off,stage.get(),0, size);
  enc->endEncoding();
  cmd->commit();

  cmd->waitUntilCompleted();
  std::memcpy(data,stage->contents(),size);
  }

void MtBuffer::implUpdate(const void *data, size_t off, size_t size) {
  auto ptr = reinterpret_cast<uint8_t*>(impl->contents());

  std::memcpy(ptr+off, data, size);
  if(impl->storageMode()!=MTL::StorageModeManaged)
    return;

  impl->didModifyRange(NS::Range(off,size));
  }

#endif
