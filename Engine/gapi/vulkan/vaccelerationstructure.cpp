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
                                       VK_ACCELERATION_STRUCTURE_BUILD_TYPE_HOST_KHR,
                                       &buildGeometryInfo,
                                       &buildGeometryInfo.geometryCount,
                                       &buildSizesInfo);

  data = dx.dataMgr().allocStagingMemory(nullptr,buildSizesInfo.accelerationStructureSize,1,1,
                                         MemUsage::AsStorage,BufferHeap::Device);

  auto  scratch = dx.dataMgr().allocStagingMemory(nullptr,buildSizesInfo.buildScratchSize,1,1,
                                                  MemUsage::ScratchBuffer,BufferHeap::Device);

  VkAccelerationStructureCreateInfoKHR createInfo = {};
  createInfo.sType         = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR;
  createInfo.pNext         = nullptr;
  createInfo.createFlags   = 0;
  createInfo.buffer        = data.impl,
  createInfo.offset        = 0;
  createInfo.size          = buildSizesInfo.accelerationStructureSize;
  createInfo.type          = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
  createInfo.deviceAddress = VK_NULL_HANDLE;
  vkCreateAccelerationStructure(device, &createInfo, nullptr, &impl);

  auto cmd = dx.dataMgr().get();
  cmd->begin();
  //cmd->hold(scratch);
  cmd->buildBlas(impl,vbo,stride,maxVertex,ibo,Detail::IndexClass::i32,primitiveCount,scratch);
  cmd->end();

  // dx.dataMgr().waitFor(this);
  dx.dataMgr().submitAndWait(std::move(cmd));
  }

VAccelerationStructure::~VAccelerationStructure() {
  auto device = owner.device.impl;
  owner.vkDestroyAccelerationStructure(device,impl,nullptr);
  }

#endif
