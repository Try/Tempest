#if defined(TEMPEST_BUILD_VULKAN)

#include "vaccelerationstructure.h"

#include "vdevice.h"
#include "vbuffer.h"

using namespace Tempest;
using namespace Tempest::Detail;

VAccelerationStructure::VAccelerationStructure(VDevice& dx,
                                               VBuffer& vbo, size_t vboSz, size_t offset, size_t stride,
                                               VBuffer& ibo, size_t iboSz, IndexClass icls)
  :owner(dx) {
  auto device                               = dx.device.impl;
  auto vkGetAccelerationStructureBuildSizes = dx.vkGetAccelerationStructureBuildSizes;
  auto vkCreateAccelerationStructure        = dx.vkCreateAccelerationStructure;

  const uint32_t maxVertex      = uint32_t(vboSz/stride);
  const uint32_t primitiveCount = uint32_t(iboSz/3u);

  VkAccelerationStructureGeometryTrianglesDataKHR trianglesData = {};
  trianglesData.sType                    = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
  trianglesData.pNext                    = nullptr;
  trianglesData.vertexFormat             = VK_FORMAT_R32G32B32_SFLOAT;
  trianglesData.vertexData.deviceAddress = vbo.toDeviceAddress(dx) + offset*stride,
  trianglesData.vertexStride             = stride;
  trianglesData.maxVertex                = maxVertex;
  trianglesData.indexType                = nativeFormat(icls);
  trianglesData.indexData.deviceAddress  = ibo.toDeviceAddress(dx);
  trianglesData.transformData            = VkDeviceOrHostAddressConstKHR{};

  VkAccelerationStructureGeometryKHR geometry = {};
  geometry.sType              = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
  geometry.pNext              = nullptr;
  geometry.geometryType       = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
  geometry.geometry.triangles = trianglesData;
  geometry.flags              = VK_GEOMETRY_OPAQUE_BIT_KHR;

  VkAccelerationStructureBuildGeometryInfoKHR buildGeometryInfo = {};
  buildGeometryInfo.sType                    = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
  buildGeometryInfo.pNext                    = nullptr;
  buildGeometryInfo.type                     = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
  buildGeometryInfo.flags                    = 0;
  buildGeometryInfo.mode                     = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
  buildGeometryInfo.srcAccelerationStructure = VK_NULL_HANDLE;
  buildGeometryInfo.dstAccelerationStructure = VK_NULL_HANDLE;
  buildGeometryInfo.geometryCount            = 1;
  buildGeometryInfo.pGeometries              = &geometry;
  buildGeometryInfo.ppGeometries             = nullptr;
  buildGeometryInfo.scratchData              = {};

  VkAccelerationStructureBuildSizesInfoKHR buildSizesInfo = {};
  buildSizesInfo.sType                     = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR;
  buildSizesInfo.pNext                     = nullptr;
  buildSizesInfo.accelerationStructureSize = 0;
  buildSizesInfo.updateScratchSize         = 0;
  buildSizesInfo.buildScratchSize          = 0;

  vkGetAccelerationStructureBuildSizes(device,
                                       VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                                       &buildGeometryInfo,
                                       &buildGeometryInfo.geometryCount,
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
  cmd->buildBlas(impl,vbo,offset,stride,maxVertex,ibo,icls,primitiveCount,scratch);
  cmd->end();

  // dx.dataMgr().waitFor(this);
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


VTopAccelerationStructure::VTopAccelerationStructure(VDevice& dx, const VAccelerationStructure* obj)
  :owner(dx) {
  auto device                               = dx.device.impl;
  auto vkGetAccelerationStructureBuildSizes = dx.vkGetAccelerationStructureBuildSizes;
  auto vkCreateAccelerationStructure        = dx.vkCreateAccelerationStructure;

  VkAccelerationStructureInstanceKHR geometryInstance = {};
  geometryInstance.transform.matrix[0][0]                 = 1.0;
  geometryInstance.transform.matrix[1][1]                 = 1.0;
  geometryInstance.transform.matrix[2][2]                 = 1.0;
  geometryInstance.instanceCustomIndex                    = 0;
  geometryInstance.mask                                   = 0xFF;
  geometryInstance.instanceShaderBindingTableRecordOffset = 0;
  geometryInstance.flags                                  = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
  geometryInstance.accelerationStructureReference         = obj->toDeviceAddress(dx);

  Detail::DSharedPtr<VBuffer*> pBuf;
  {
  VBuffer buf = dx.allocator.alloc(nullptr,1,sizeof(geometryInstance),sizeof(geometryInstance),
                                   MemUsage::TransferDst | MemUsage::ScratchBuffer,BufferHeap::Device);

  pBuf = Detail::DSharedPtr<VBuffer*>(new Detail::VBuffer(std::move(buf)));
  pBuf.handler->update(&geometryInstance,0,1, sizeof(geometryInstance), sizeof(geometryInstance));
  }

  VkAccelerationStructureGeometryInstancesDataKHR geometryInstancesData = {};
  geometryInstancesData.sType                = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
  geometryInstancesData.pNext                = NULL;
  geometryInstancesData.arrayOfPointers      = VK_FALSE;
  geometryInstancesData.data.deviceAddress   = pBuf.handler->toDeviceAddress(dx);

  VkAccelerationStructureGeometryKHR geometry = {};
  geometry.sType                             = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
  geometry.pNext                             = nullptr;
  geometry.geometryType                      = VK_GEOMETRY_TYPE_INSTANCES_KHR;
  geometry.geometry.instances                = geometryInstancesData;
  geometry.flags                             = VK_GEOMETRY_OPAQUE_BIT_KHR;

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

  vkGetAccelerationStructureBuildSizes(device,
                                       VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
                                       &buildGeometryInfo,
                                       &buildGeometryInfo.geometryCount,
                                       &buildSizesInfo);

  data = dx.allocator.alloc(nullptr, buildSizesInfo.accelerationStructureSize,1,1, MemUsage::AsStorage,BufferHeap::Device);

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
  cmd->buildTlas(impl,data,*pBuf.handler,1,scratch);
  cmd->end();

  // dx.dataMgr().waitFor(this);
  dx.dataMgr().submitAndWait(std::move(cmd));
  }

VTopAccelerationStructure::~VTopAccelerationStructure() {
  auto device = owner.device.impl;
  owner.vkDestroyAccelerationStructure(device,impl,nullptr);
  }

#endif
