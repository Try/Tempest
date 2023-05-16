#if defined(TEMPEST_BUILD_VULKAN)

#include "vaccelerationstructure.h"

#include "vdevice.h"
#include "vbuffer.h"

using namespace Tempest;
using namespace Tempest::Detail;

VAccelerationStructure::VAccelerationStructure(VDevice& dx,
                                               VBuffer& vbo, size_t vboSz, size_t stride,
                                               VBuffer& ibo, size_t iboSz, size_t ioffset, IndexClass icls)
  :owner(dx) {
  auto device                               = dx.device.impl;
  auto vkGetAccelerationStructureBuildSizes = dx.vkGetAccelerationStructureBuildSizes;
  auto vkCreateAccelerationStructure        = dx.vkCreateAccelerationStructure;

  VkAccelerationStructureGeometryKHR geometry = {};
  geometry.sType                             = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
  geometry.geometryType                      = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
  geometry.geometry.triangles.sType          = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
  geometry.geometry.triangles.vertexFormat   = VK_FORMAT_R32G32B32_SFLOAT;
  geometry.geometry.triangles.vertexData     = VkDeviceOrHostAddressConstKHR{};
  geometry.geometry.triangles.vertexStride   = stride;
  geometry.geometry.triangles.maxVertex      = uint32_t(vboSz);
  geometry.geometry.triangles.indexType      = nativeFormat(icls);
  geometry.geometry.triangles.indexData      = VkDeviceOrHostAddressConstKHR{};
  geometry.geometry.triangles.transformData  = VkDeviceOrHostAddressConstKHR{};
  geometry.flags                             = 0;

  VkAccelerationStructureBuildGeometryInfoKHR buildGeometryInfo = {};
  buildGeometryInfo.sType                    = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
  buildGeometryInfo.pNext                    = nullptr;
  buildGeometryInfo.type                     = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
  buildGeometryInfo.flags                    = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
  buildGeometryInfo.mode                     = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
  buildGeometryInfo.srcAccelerationStructure = VK_NULL_HANDLE;
  buildGeometryInfo.dstAccelerationStructure = VK_NULL_HANDLE;
  buildGeometryInfo.geometryCount            = 1;
  buildGeometryInfo.pGeometries              = &geometry;
  buildGeometryInfo.ppGeometries             = nullptr;
  buildGeometryInfo.scratchData              = {};

  VkAccelerationStructureBuildSizesInfoKHR buildSizesInfo = {};
  buildSizesInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;

  const uint32_t maxPrimitiveCounts = uint32_t(iboSz/3);
  vkGetAccelerationStructureBuildSizes(device,
                                       VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                                       &buildGeometryInfo,
                                       &maxPrimitiveCounts,
                                       &buildSizesInfo);

  data = dx.allocator.alloc(nullptr, buildSizesInfo.accelerationStructureSize,1,1, MemUsage::AsStorage,BufferHeap::Device);

  auto  scratch = dx.dataMgr().allocStagingMemory(nullptr,buildSizesInfo.buildScratchSize,1,1,
                                                  MemUsage::ScratchBuffer,BufferHeap::Device);

  VkAccelerationStructureCreateInfoKHR createInfo = {};
  createInfo.sType         = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
  createInfo.pNext         = nullptr;
  createInfo.createFlags   = 0;
  createInfo.buffer        = data.impl;
  createInfo.offset        = 0;
  createInfo.size          = buildSizesInfo.accelerationStructureSize;
  createInfo.type          = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
  createInfo.deviceAddress = VK_NULL_HANDLE;
  vkCreateAccelerationStructure(device, &createInfo, nullptr, &impl);

  auto cmd = dx.dataMgr().get();
  cmd->begin();
  //cmd->hold(scratch);
  cmd->buildBlas(impl,
                 vbo,vboSz,stride,
                 ibo,iboSz,ioffset,icls, scratch);
  cmd->end();

  // dx.dataMgr().waitFor(this);
  dx.dataMgr().waitFor(&vbo);
  dx.dataMgr().waitFor(&ibo);

  dx.dataMgr().submitAndWait(std::move(cmd));
  }

VAccelerationStructure::~VAccelerationStructure() {
  auto device = owner.device.impl;
  owner.vkDestroyAccelerationStructure(device,impl,nullptr);
  }

VkDeviceAddress VAccelerationStructure::toDeviceAddress(VDevice& dx) const {
  auto vkGetAccelerationStructureDeviceAddress = dx.vkGetAccelerationStructureDeviceAddress;

  VkAccelerationStructureDeviceAddressInfoKHR accelerationStructureDeviceAddressInfo = {};
  accelerationStructureDeviceAddressInfo.sType                 = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR;
  accelerationStructureDeviceAddressInfo.accelerationStructure = impl;

  return vkGetAccelerationStructureDeviceAddress(dx.device.impl, &accelerationStructureDeviceAddressInfo);
  }


VTopAccelerationStructure::VTopAccelerationStructure(VDevice& dx, const RtInstance* inst, AccelerationStructure*const* as, size_t asSize)
  :owner(dx) {
  auto device                               = dx.device.impl;
  auto vkGetAccelerationStructureBuildSizes = dx.vkGetAccelerationStructureBuildSizes;
  auto vkCreateAccelerationStructure        = dx.vkCreateAccelerationStructure;

  VkAccelerationStructureGeometryInstancesDataKHR geometryInstancesData = {};
  geometryInstancesData.sType                = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
  geometryInstancesData.pNext                = nullptr;
  geometryInstancesData.arrayOfPointers      = VK_FALSE;
  geometryInstancesData.data.deviceAddress   = {};

  VkAccelerationStructureGeometryKHR geometry = {};
  geometry.sType                             = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
  geometry.pNext                             = nullptr;
  geometry.geometryType                      = VK_GEOMETRY_TYPE_INSTANCES_KHR;
  geometry.geometry.instances                = geometryInstancesData;
  geometry.flags                             = 0;

  VkAccelerationStructureBuildGeometryInfoKHR buildGeometryInfo = {};
  buildGeometryInfo.sType                    = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
  buildGeometryInfo.pNext                    = nullptr;
  buildGeometryInfo.type                     = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
  buildGeometryInfo.flags                    = VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR;
  buildGeometryInfo.mode                     = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
  buildGeometryInfo.srcAccelerationStructure = VK_NULL_HANDLE;
  buildGeometryInfo.dstAccelerationStructure = VK_NULL_HANDLE;
  buildGeometryInfo.geometryCount            = 1;
  buildGeometryInfo.pGeometries              = &geometry;
  buildGeometryInfo.ppGeometries             = nullptr;
  buildGeometryInfo.scratchData              = {};

  VkAccelerationStructureBuildSizesInfoKHR buildSizesInfo = {};
  buildSizesInfo.sType                       = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
  buildSizesInfo.pNext                       = nullptr;

  uint32_t numInstances = uint32_t(asSize);
  vkGetAccelerationStructureBuildSizes(device,
                                       VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                                       &buildGeometryInfo,
                                       &numInstances,
                                       &buildSizesInfo);

  data = dx.allocator.alloc(nullptr, buildSizesInfo.accelerationStructureSize,1,1, MemUsage::AsStorage,BufferHeap::Device);

  Detail::DSharedPtr<VBuffer*> pBuf;
  if(asSize>0) {
    VBuffer buf = dx.allocator.alloc(nullptr,asSize,sizeof(VkAccelerationStructureInstanceKHR),sizeof(VkAccelerationStructureInstanceKHR),
                                     MemUsage::TransferDst | MemUsage::StorageBuffer,BufferHeap::Upload);
    pBuf = Detail::DSharedPtr<VBuffer*>(new Detail::VBuffer(std::move(buf)));
    }

  for(size_t i=0; i<asSize; ++i) {
    auto blas = reinterpret_cast<VAccelerationStructure*>(as[i]);

    VkAccelerationStructureInstanceKHR objInstance = {};
    for(int x=0; x<3; ++x)
      for(int y=0; y<4; ++y)
        objInstance.transform.matrix[x][y] = inst[i].mat.at(y,x);
    objInstance.instanceCustomIndex                    = inst[i].id;
    objInstance.mask                                   = 0xFF;
    objInstance.instanceShaderBindingTableRecordOffset = 0;
    objInstance.flags                                  = nativeFormat(inst[i].flags);
    objInstance.accelerationStructureReference         = blas->toDeviceAddress(dx);

    pBuf.handler->update(&objInstance,i, 1,sizeof(objInstance), sizeof(objInstance));
    }

  auto  scratch = dx.dataMgr().allocStagingMemory(nullptr,buildSizesInfo.buildScratchSize,1,1,
                                                  MemUsage::ScratchBuffer,BufferHeap::Device);

  VkAccelerationStructureCreateInfoKHR createInfo = {};
  createInfo.sType         = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
  createInfo.pNext         = nullptr;
  createInfo.createFlags   = 0;
  createInfo.buffer        = data.impl;
  createInfo.offset        = 0;
  createInfo.size          = buildSizesInfo.accelerationStructureSize;
  createInfo.type          = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
  createInfo.deviceAddress = VK_NULL_HANDLE;
  vkCreateAccelerationStructure(device, &createInfo, nullptr, &impl);

  auto cmd = dx.dataMgr().get();
  cmd->begin();
  //cmd->hold(scratch);
  cmd->buildTlas(impl,data,*pBuf.handler,uint32_t(asSize),scratch);
  cmd->end();

  // dx.dataMgr().waitFor(this);
  dx.dataMgr().submitAndWait(std::move(cmd));
  }

VTopAccelerationStructure::~VTopAccelerationStructure() {
  auto device = owner.device.impl;
  owner.vkDestroyAccelerationStructure(device,impl,nullptr);
  }

#endif
