#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <vulkan/vulkan.hpp>
#include "gapi/deviceallocator.h"

namespace Tempest {
namespace Detail {

class VDevice;
class VBuffer;
class VTexture;

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

    VBuffer  alloc(const void *mem,  size_t size, MemUsage usage, BufferFlags bufFlg);
    VTexture alloc(const Pixmap &pm, bool mip);
    void     free(VBuffer&  buf);
    void     free(VTexture& buf);

  private:
    VkDevice                          device=nullptr;
    Provider                          provider;
    Detail::DeviceAllocator<Provider> allocator{provider};

    bool commit(VkDeviceMemory dev,VkBuffer dest,const void *mem,size_t offset,size_t size);
    bool commit(VkDeviceMemory dev, VkImage  dest, size_t offset);
  };

}}
