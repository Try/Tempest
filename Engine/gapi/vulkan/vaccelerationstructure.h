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
    VAccelerationStructure(VDevice& owner, VBuffer& vbo, size_t stride, VBuffer& ibo, Detail::IndexClass icls);
    ~VAccelerationStructure();

    VDevice&                   owner;
    VkAccelerationStructureKHR impl = VK_NULL_HANDLE;
    VBuffer                    data;
  };

}
}
