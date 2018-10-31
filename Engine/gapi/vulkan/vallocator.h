#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <vulkan/vulkan.hpp>
#include "gapi/deviceallocator.h"

namespace Tempest {
namespace Detail {

class VDevice;
class VBuffer;

class VAllocator {
  private:
    struct Provider {
      using DeviceMemory=VkDeviceMemory;

      VDevice*     device=nullptr;
      DeviceMemory alloc(size_t size, uint32_t typeId);
      void         free(DeviceMemory m);
      };

  public:
    VAllocator();

    void setDevice(VDevice& device);

    using Allocation=typename Tempest::Detail::DeviceAllocator<Provider>::Allocation;

    VBuffer alloc(const void *mem, size_t size, MemUsage usage, BufferFlags bufFlg);
    void    free(VBuffer& buf);

  private:
    VkDevice                          device=nullptr;
    Provider                          provider;
    Detail::DeviceAllocator<Provider> allocator{provider};

    bool commit(VkDeviceMemory dev,VkBuffer dest,const void *mem,size_t offset,size_t size);
  };

}}
