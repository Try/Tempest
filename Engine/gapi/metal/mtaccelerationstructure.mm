#include "mtaccelerationstructure.h"

#include "mtdevice.h"
#include "mtbuffer.h"

#include "utility/compiller_hints.h"
#include "gapi/graphicsmemutils.h"

#import <Metal/MTLDevice.h>
#import <Metal/MTLCommandQueue.h>
#import <Metal/MTLCommandBuffer.h>

using namespace Tempest::Detail;

MtAccelerationStructure::MtAccelerationStructure(MtDevice& dx,
                                                 MtBuffer& vbo, size_t vboSz, size_t stride,
                                                 MtBuffer& ibo, size_t iboSz, size_t ioffset, IndexClass icls)
  :owner(dx) {
  auto* geo = [MTLAccelerationStructureTriangleGeometryDescriptor new];
  geo.vertexBuffer       = vbo.impl;
  geo.vertexBufferOffset = 0;
  geo.vertexStride       = stride;
  geo.indexBuffer        = ibo.impl;
  geo.indexBufferOffset  = ioffset*sizeofIndex(icls);
  geo.indexType          = nativeFormat(icls);
  geo.triangleCount      = iboSz/3;

  NSArray *geoArr = @[geo];

  auto* desc = [MTLPrimitiveAccelerationStructureDescriptor new];
  desc.geometryDescriptors = geoArr;
  desc.usage               = MTLAccelerationStructureUsageNone;

  MTLAccelerationStructureSizes sz = [dx.impl accelerationStructureSizesWithDescriptor:desc];

  impl = [dx.impl newAccelerationStructureWithSize:sz.accelerationStructureSize];
  if(impl==nil)
    throw std::system_error(GraphicsErrc::OutOfVideoMemory);

  id<MTLBuffer> scratch = [dx.impl newBufferWithLength:sz.buildScratchBufferSize options:MTLResourceStorageModePrivate];
  if(scratch==nil && sz.buildScratchBufferSize>0) {
    [impl release];
    throw std::system_error(GraphicsErrc::OutOfVideoMemory);
    }

  id<MTLCommandBuffer>                       cmd = [dx.queue commandBuffer];
  id<MTLAccelerationStructureCommandEncoder> enc = [cmd accelerationStructureCommandEncoder];
  [enc buildAccelerationStructure:impl
                       descriptor:desc
                    scratchBuffer:scratch
              scratchBufferOffset:0];
  [enc endEncoding];
  [cmd commit];
  // TODO: implement proper upload engine
  [cmd waitUntilCompleted];

  if(scratch!=nil)
    [scratch release];
  }

MtAccelerationStructure::~MtAccelerationStructure() {
  [impl release];
  }


MtTopAccelerationStructure::MtTopAccelerationStructure(MtDevice& dx, const RtInstance* inst, AccelerationStructure*const* as, size_t asSize)
  :owner(dx) {
  NSMutableArray* asArray = nil;
  id<MTLBuffer>   scratch = nil;
  try {
    instances = [dx.impl newBufferWithLength:sizeof(MTLAccelerationStructureInstanceDescriptor)*asSize
                                     options:MTLResourceStorageModeManaged];
    if(instances==nil)
      throw std::system_error(GraphicsErrc::OutOfVideoMemory);

    asArray = [NSMutableArray arrayWithCapacity:asSize];
    if(asArray==nil && asSize>0)
      throw std::system_error(GraphicsErrc::OutOfHostMemory);

    auto desc = [MTLInstanceAccelerationStructureDescriptor new];
    if(desc==nil)
      throw std::system_error(GraphicsErrc::OutOfHostMemory);

    desc.instanceDescriptorBuffer        = instances;
    desc.instanceDescriptorBufferOffset  = 0;
    desc.instanceDescriptorStride        = sizeof(MTLAccelerationStructureInstanceDescriptor);
    desc.instanceCount                   = asSize;
    desc.instancedAccelerationStructures = asArray;

    MTLAccelerationStructureSizes sz = [dx.impl accelerationStructureSizesWithDescriptor:desc];

    impl = [dx.impl newAccelerationStructureWithSize:sz.accelerationStructureSize];
    if(impl==nil)
      throw std::system_error(GraphicsErrc::OutOfVideoMemory);

    for(size_t i=0; i<asSize; ++i) {
      auto& obj = reinterpret_cast<MTLAccelerationStructureInstanceDescriptor*>([instances contents])[i];

      for(int x=0; x<3; ++x)
        for(int y=0; y<4; ++y)
          obj.transformationMatrix[x][y] = inst[i].mat.at(y,x);
      obj.options                         = MTLAccelerationStructureInstanceOptionDisableTriangleCulling |
                                            MTLAccelerationStructureInstanceOptionOpaque;
      obj.mask                            = 0xFF;
      obj.intersectionFunctionTableOffset = 0;
      obj.accelerationStructureIndex      = i;

      auto* ax = reinterpret_cast<MtAccelerationStructure*>(as[i]);
      [asArray addObject:ax->impl];
      }
    [instances didModifyRange:NSMakeRange(0,[instances length])];

    id<MTLBuffer> scratch = [dx.impl newBufferWithLength:sz.buildScratchBufferSize options:MTLResourceStorageModePrivate];
    if(scratch==nil && sz.buildScratchBufferSize>0)
      throw std::system_error(GraphicsErrc::OutOfVideoMemory);

    id<MTLCommandBuffer>                       cmd = [dx.queue commandBuffer];
    id<MTLAccelerationStructureCommandEncoder> enc = [cmd accelerationStructureCommandEncoder];
    [enc buildAccelerationStructure:impl
                         descriptor:desc
                      scratchBuffer:scratch
                scratchBufferOffset:0];
    [enc endEncoding];
    [cmd commit];
    // TODO: implement proper upload engine
    [cmd waitUntilCompleted];
    }
  catch(...) {
    if(asArray!=nil)
      [asArray release];
    if(scratch!=nil)
      [scratch release];
    if(impl!=nil)
      [impl release];
    if(instances!=nil)
      [instances release];
    throw;
    }

  if(scratch!=nil)
    [scratch release];
  if(asArray!=nil)
    [asArray release];
  }

MtTopAccelerationStructure::~MtTopAccelerationStructure() {
  [instances release];
  [impl release];
  }
