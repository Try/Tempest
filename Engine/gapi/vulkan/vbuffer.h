#pragma once

#include <Tempest/AbstractGraphicsApi>
#include "vulkan_sdk.h"

#include "vallocator.h"

namespace Tempest {
namespace Detail {

class VBuffer : public AbstractGraphicsApi::Buffer {
  public:
    VBuffer()=default;
    VBuffer(VBuffer &&other);
    ~VBuffer();

    VBuffer& operator=(VBuffer&& other);

    void update  (const void* data, size_t off, size_t count, size_t sz, size_t alignedSz) override;
    void read    (void* data, size_t off, size_t sz) override;

    VkDeviceAddress        toDeviceAddress(VDevice& owner) const;
    VkBuffer               impl      = VK_NULL_HANDLE;

  private:
    VAllocator*            alloc=nullptr;
    VAllocator::Allocation page={};

  friend class VAllocator;
  };

}}
