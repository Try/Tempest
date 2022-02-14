#include "vaccelerationstructure.h"

#include "vulkan_sdk.h"
#include "vdevice.h"
#include "vbuffer.h"

using namespace Tempest;
using namespace Tempest::Detail;

VAccelerationStructure::VAccelerationStructure(VDevice& owner, VBuffer& vbo, VBuffer& ibo) {
  auto device                               = owner.device.impl;
  auto vkGetAccelerationStructureBuildSizes = owner.vkGetAccelerationStructureBuildSizes;


  VkAccelerationStructureGeometryTrianglesDataKHR trianglesData = {};
  trianglesData.sType                    = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
  trianglesData.pNext                    = nullptr;
  trianglesData.vertexFormat             = VK_FORMAT_R32G32B32_SFLOAT;
  trianglesData.vertexData.deviceAddress = vbo.toDeviceAddress(owner),
  trianglesData.vertexStride             = sizeof(float) * 3;
  trianglesData.maxVertex                = 3;//scene->attributes.num_vertices; // TODO
  trianglesData.indexType                = VK_INDEX_TYPE_UINT32;
  trianglesData.indexData.deviceAddress  = ibo.toDeviceAddress(owner);
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

  // auto scratch = owner.;
  }
