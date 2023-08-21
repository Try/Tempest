#pragma once

#include <Tempest/AbstractGraphicsApi>

#include "vulkan_sdk.h"
#include "vbuffer.h"

namespace Tempest {
namespace Detail {

class VDevice;
class VBuffer;

struct VBlasBuildCtx : AbstractGraphicsApi::BlasBuildCtx {
  void pushGeometry(VDevice& dx,
                    const VBuffer& vbo, size_t vboSz, size_t stride,
                    const VBuffer& ibo, size_t iboSz, size_t ioffset, IndexClass icls);

  VkAccelerationStructureBuildSizesInfoKHR    buildSizes(VDevice& dx) const;
  VkAccelerationStructureBuildGeometryInfoKHR buildCmd  (VDevice& dx, VkAccelerationStructureKHR dest, VBuffer* scratch) const;

  std::vector<VkAccelerationStructureBuildRangeInfoKHR> ranges;
  std::vector<VkAccelerationStructureGeometryKHR>       geometry;
  std::vector<uint32_t>                                 maxPrimitiveCounts;
  };

class VAccelerationStructure : public AbstractGraphicsApi::AccelerationStructure {
  public:
    VAccelerationStructure(VDevice& owner, const AbstractGraphicsApi::RtGeometry* geom, size_t size);
    ~VAccelerationStructure();

    VkDeviceAddress            toDeviceAddress(VDevice& owner) const;

    VDevice&                   owner;
    VkAccelerationStructureKHR impl = VK_NULL_HANDLE;
    VBuffer                    data;
  };

class VTopAccelerationStructure : public AbstractGraphicsApi::AccelerationStructure {
  public:
    VTopAccelerationStructure(VDevice& owner, const RtInstance* inst, AccelerationStructure* const * as, size_t size);
    ~VTopAccelerationStructure();

    VDevice&                   owner;
    VkAccelerationStructureKHR impl = VK_NULL_HANDLE;
    VBuffer                    data;
  };

}
}
