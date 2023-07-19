#if defined(TEMPEST_BUILD_METAL)

#include "mtaccelerationstructure.h"

#include "mtdevice.h"
#include "mtbuffer.h"

#include "utility/compiller_hints.h"

#include <Tempest/Log>
#include <unordered_map>

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
  geo->setVertexFormat(MTL::AttributeFormatFloat3);
  geo->setIndexBuffer(ibo.impl.get());
  geo->setIndexBufferOffset(ioffset*sizeofIndex(icls));
  geo->setIndexType(nativeFormat(icls));
  geo->setTriangleCount(iboSz/3);
  geo->setOpaque(true);
  geo->setAllowDuplicateIntersectionFunctionInvocation(true);

  if(geo->indexBufferOffset()%32!=0) {
    Log::d("FIXME: index buffer offset alignment on metal(",geo->indexBufferOffset()%32,")");
    geo->setIndexBufferOffset(0);
    geo->setTriangleCount(0);
    }

  auto geoArr = NsPtr<NS::Array>(NS::Array::array(geo.get()));
  auto desc   = NsPtr<MTL::PrimitiveAccelerationStructureDescriptor>::init();
  desc->retain();
  desc->setGeometryDescriptors(geoArr.get());
  desc->setUsage(MTL::AccelerationStructureUsageNone);

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
  instances = NsPtr<MTL::Buffer>(dx.impl->newBuffer(sizeof(MTL::AccelerationStructureUserIDInstanceDescriptor)*asSize,
                                                    MTL::ResourceStorageModeManaged));
  if(instances==nullptr)
    throw std::system_error(GraphicsErrc::OutOfVideoMemory);

  std::unordered_map<MTL::Resource*,uint32_t> usedBlas;
  blas.reserve(asSize);
  for(size_t i=0; i<asSize; ++i) {
    auto& obj = reinterpret_cast<MTL::AccelerationStructureUserIDInstanceDescriptor*>(instances->contents())[i];

    for(int x=0; x<4; ++x)
      for(int y=0; y<3; ++y)
        obj.transformationMatrix[x][y] = inst[i].mat.at(x,y);
    obj.options                         = nativeFormat(inst[i].flags);
    obj.mask                            = 0xFF;
    obj.intersectionFunctionTableOffset = 0;
    obj.accelerationStructureIndex      = i;
    obj.userID                          = inst[i].id;

    auto* ax = reinterpret_cast<MtAccelerationStructure*>(as[i]);
    auto it = usedBlas.find(ax->impl.get());
    if(it!=usedBlas.end()) {
      obj.accelerationStructureIndex = it->second;
      } else {
      obj.accelerationStructureIndex = uint32_t(blas.size());
      usedBlas[ax->impl.get()] = blas.size();
      blas.push_back(ax->impl.get());
      }
    }
  instances->didModifyRange(NS::Range(0,instances->length()));

  auto asArray = NsPtr<NS::Array>(NS::Array::array(reinterpret_cast<NS::Object**>(blas.data()), blas.size()));
  auto desc    = NsPtr<MTL::InstanceAccelerationStructureDescriptor>::init();
  if(desc==nullptr)
    throw std::system_error(GraphicsErrc::OutOfHostMemory);
  desc->retain();
  desc->setInstanceDescriptorBuffer(instances.get());
  desc->setInstanceDescriptorBufferOffset(0);
  desc->setInstanceDescriptorStride(sizeof(MTL::AccelerationStructureUserIDInstanceDescriptor));
  desc->setInstanceCount(asSize);
  desc->setInstancedAccelerationStructures(asArray.get());
  desc->setInstanceDescriptorType(MTL::AccelerationStructureInstanceDescriptorTypeUserID);

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

#endif
