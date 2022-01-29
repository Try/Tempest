#pragma once

#include <Tempest/AbstractGraphicsApi>
#include "vulkan_sdk.h"
#include "gapi/deviceallocator.h"
#include "vsamplercache.h"

namespace Tempest {
namespace Detail {

class VDevice;
class VBuffer;
class VTexture;

class VAllocator {
  private:
    struct Provider {
      using DeviceMemory=VkDeviceMemory;
      ~Provider();

      VDevice*     device=nullptr;

      DeviceMemory lastFree=VK_NULL_HANDLE;
      uint32_t     lastType=0;
      size_t       lastSize=0;

      DeviceMemory alloc(size_t size, uint32_t typeId);
      void         free(DeviceMemory m, size_t size, uint32_t typeId);
      };

    struct MemRequirements {
      size_t               size;
      size_t               alignment;
      uint32_t             memoryTypeBits;
      bool                 dedicated;
      bool                 dedicatedRq;
      };

  public:
    VAllocator();
    ~VAllocator();

    void setDevice(VDevice& device);
    VDevice* device();

    using Allocation=typename Tempest::Detail::DeviceAllocator<Provider>::Allocation;

    VBuffer  alloc(const void *mem, size_t count, size_t size, size_t alignedSz, MemUsage usage, BufferHeap bufHeap);
    VTexture alloc(const Pixmap &pm, uint32_t mip, VkFormat format);
    VTexture alloc(const uint32_t w, const uint32_t h, const uint32_t mip, TextureFormat frm, bool imgStorage);
    void     free(VBuffer&  buf);
    void     free(VTexture& buf);

    bool     update(VBuffer& dest, const void *mem, size_t offset, size_t count, size_t size, size_t alignedSz);
    bool     read  (VBuffer& src,        void *mem, size_t offset, size_t count, size_t size, size_t alignedSz);
    bool     read  (VBuffer& src,        void *mem, size_t offset, size_t size);

    VkSampler updateSampler(const Sampler2d& s);

  private:
    VkDevice                          dev=nullptr;
    Provider                          provider;
    VSamplerCache                     samplers;
    Detail::DeviceAllocator<Provider> allocator{provider};

    void getMemoryRequirements   (MemRequirements& out, VkBuffer buf);
    void getImgMemoryRequirements(MemRequirements& out, VkImage  img);
    void alignRange(VkMappedMemoryRange& rgn, size_t nonCoherentAtomSize, size_t &shift);

    Allocation allocMemory(const MemRequirements& rq, const uint32_t heapId, const uint32_t typeId, bool hostVisible);

    bool commit(VkDeviceMemory dev, std::mutex& mmapSync, VkBuffer dest, size_t offset,
                const void *mem, size_t count, size_t size, size_t alignedSz);
    bool commit(VkDeviceMemory dev, std::mutex& mmapSync, VkImage  dest, size_t offset);
  };

}}
