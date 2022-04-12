#pragma once

#include <Tempest/AbstractGraphicsApi>

#include "vulkan_sdk.h"
#include "vbuffer.h"

namespace Tempest {
namespace Detail {

class VDevice;
class VBuffer;

class VAccelerationStructure : public AbstractGraphicsApi::AccelerationStructure {
  public:
    VAccelerationStructure(VDevice& owner,
                           VBuffer& vbo, size_t vboSz, size_t stride,
                           VBuffer& ibo, size_t iboSz, size_t ioffset, Detail::IndexClass icls);
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
