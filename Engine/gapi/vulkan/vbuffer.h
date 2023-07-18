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
    void update  (const void* data, size_t off, size_t size) override;
    void read    (void* data, size_t off, size_t sz) override;

    bool                   isHostVisible() const;

    VkDeviceAddress        toDeviceAddress(VDevice& owner) const;
    VkBuffer               impl      = VK_NULL_HANDLE;
    NonUniqResId           nonUniqId = NonUniqResId::I_None;

  private:
    VAllocator*            alloc=nullptr;
    VAllocator::Allocation page={};

  friend class VAllocator;
  };

}}
