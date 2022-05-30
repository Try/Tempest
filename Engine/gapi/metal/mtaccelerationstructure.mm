#include "mtaccelerationstructure.h"

#include "mtdevice.h"
#include "mtbuffer.h"

#include "utility/compiller_hints.h"
#include "gapi/graphicsmemutils.h"

#include <Tempest/Log>

using namespace Tempest::Detail;

MtAccelerationStructure::MtAccelerationStructure(MtDevice& dx,
                                                 MtBuffer& vbo, size_t vboSz, size_t stride,
                                                 MtBuffer& ibo, size_t iboSz, size_t ioffset, IndexClass icls)
  :owner(dx) {
  auto pool = NsPtr<NS::AutoreleasePool>::init();
  auto geo  = NsPtr<MTL::AccelerationStructureTriangleGeometryDescriptor>::init();
  if(geo==nullptr)
    throw std::system_error(GraphicsErrc::OutOfHostMemory);
  geo->setVertexBuffer(vbo.impl.get());
  geo->setVertexBufferOffset(0);
  geo->setVertexStride(stride);
  geo->setIndexBuffer(ibo.impl.get());
  geo->setIndexBufferOffset(ioffset*sizeofIndex(icls));
  geo->setIndexType(nativeFormat(icls));
  geo->setTriangleCount(iboSz/3);

  if(geo->indexBufferOffset()%256!=0) {
    //Log::d("FIXME: index buffer offset alignment on metal(",geo.indexBufferOffset%256,")");
    //geo.indexBufferOffset = 0;
    //geo.triangleCount     = 0;
    }

  auto geoArr = NsPtr<NS::Array>(NS::Array::array(geo.get()));
  auto desc   = NsPtr<MTL::PrimitiveAccelerationStructureDescriptor>::init();
  desc->retain();
  desc->setGeometryDescriptors(geoArr.get());
  desc->setUsage(MTL::AccelerationStructureUsageNone);

  if(@available(macOS 12.0, *)) {
    // ???
    //desc.usage = MTLAccelerationStructureUsageExtendedLimits;
    }

  auto sz = dx.impl->accelerationStructureSizes(desc.get());

  impl = NsPtr<MTL::AccelerationStructure>(dx.impl->newAccelerationStructure(sz.accelerationStructureSize));
  if(impl==nullptr)
    throw std::system_error(GraphicsErrc::OutOfVideoMemory);

  NsPtr<MTL::Buffer> scratch = NsPtr<MTL::Buffer>(dx.impl->newBuffer(sz.buildScratchBufferSize, MTL::ResourceStorageModePrivate));
  if(scratch==nil && sz.buildScratchBufferSize>0)
    throw std::system_error(GraphicsErrc::OutOfVideoMemory);

  auto cmd = dx.queue->commandBuffer();
  auto enc = cmd->accelerationStructureCommandEncoder();
  enc->buildAccelerationStructure(impl.get(), desc.get(), scratch.get(), 0);
  enc->endEncoding();
  cmd->commit();
  // TODO: implement proper upload engine
  cmd->waitUntilCompleted();

  MTL::CommandBufferStatus s = cmd->status();
  if(s!=MTL::CommandBufferStatus::CommandBufferStatusCompleted)
    throw DeviceLostException();
  }

MtAccelerationStructure::~MtAccelerationStructure() {
  }


MtTopAccelerationStructure::MtTopAccelerationStructure(MtDevice& dx, const RtInstance* inst,
                                                       AccelerationStructure*const* as, size_t asSize)
  :owner(dx) {
  auto pool = NsPtr<NS::AutoreleasePool>::init();
  instances = NsPtr<MTL::Buffer>(dx.impl->newBuffer(sizeof(MTL::AccelerationStructureInstanceDescriptor)*asSize,
                                                    MTL::ResourceStorageModeManaged));
  if(instances==nullptr)
    throw std::system_error(GraphicsErrc::OutOfVideoMemory);

  std::unique_ptr<NS::Object*[]> asCpp;
  asCpp.reset(new NS::Object*[asSize]);
  for(size_t i=0; i<asSize; ++i) {
    auto& obj = reinterpret_cast<MTL::AccelerationStructureInstanceDescriptor*>(instances->contents())[i];

    for(int x=0; x<4; ++x)
      for(int y=0; y<3; ++y)
        obj.transformationMatrix[x][y] = inst[i].mat.at(x,y);
    obj.options                         = MTL::AccelerationStructureInstanceOptionDisableTriangleCulling |
                                          MTL::AccelerationStructureInstanceOptionOpaque;
    obj.mask                            = 0xFF;
    obj.intersectionFunctionTableOffset = 0;
    obj.accelerationStructureIndex      = i;

    auto* ax = reinterpret_cast<MtAccelerationStructure*>(as[i]);
    asCpp[i] = ax->impl.get();
    }
  instances->didModifyRange(NS::Range(0,instances->length()));

  auto asArray = NsPtr<NS::Array>(NS::Array::array(asCpp.get(), asSize));
  auto desc    = NsPtr<MTL::InstanceAccelerationStructureDescriptor>::init();
  if(desc==nullptr)
    throw std::system_error(GraphicsErrc::OutOfHostMemory);
  desc->retain();
  desc->setInstanceDescriptorBuffer(instances.get());
  desc->setInstanceDescriptorBufferOffset(0);
  desc->setInstanceDescriptorStride(sizeof(MTL::AccelerationStructureInstanceDescriptor));
  desc->setInstanceCount(asSize);
  desc->setInstancedAccelerationStructures(asArray.get());

  auto sz = dx.impl->accelerationStructureSizes(desc.get());
  impl = NsPtr<MTL::AccelerationStructure>(dx.impl->newAccelerationStructure(sz.accelerationStructureSize));
  if(impl==nullptr)
    throw std::system_error(GraphicsErrc::OutOfVideoMemory);

  auto scratch = NsPtr<MTL::Buffer>(dx.impl->newBuffer(sz.buildScratchBufferSize,MTL::ResourceStorageModePrivate));
  if(scratch==nullptr && sz.buildScratchBufferSize>0)
    throw std::system_error(GraphicsErrc::OutOfVideoMemory);

  auto cmd = dx.queue->commandBuffer();
  auto enc = cmd->accelerationStructureCommandEncoder();
  enc->buildAccelerationStructure(impl.get(), desc.get(), scratch.get(), 0);
  enc->endEncoding();
  cmd->commit();
  // TODO: implement proper upload engine
  cmd->waitUntilCompleted();
  }

MtTopAccelerationStructure::~MtTopAccelerationStructure() {
  }
