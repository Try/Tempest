#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <vulkan/vulkan.hpp>

#include "vallocator.h"

namespace Tempest {
namespace Detail {

class VBuffer : public AbstractGraphicsApi::Buffer {
  public:
    VBuffer()=default;
    VBuffer(VBuffer &&other);
    ~VBuffer();

    VBuffer& operator=(const VBuffer& other)=delete;
    void update(const void* data,size_t off,size_t sz);
    void read  (void* data,size_t off,size_t sz);

    VkBuffer               impl=VK_NULL_HANDLE;

  private:
    VAllocator*            alloc=nullptr;
    VAllocator::Allocation page={};

  friend class VAllocator;
  };

}}
