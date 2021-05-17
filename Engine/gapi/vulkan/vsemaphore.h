#pragma once

#include <Tempest/AbstractGraphicsApi>
#include "vulkan_sdk.h"

namespace Tempest {
namespace Detail {

class VDevice;

class VSemaphore : public AbstractGraphicsApi::Semaphore {
  public:
    VSemaphore(VDevice& dev);
    VSemaphore(VSemaphore&& other);
    ~VSemaphore();

    VkSemaphore impl  = VK_NULL_HANDLE;

  private:
    VkDevice    device=nullptr;
  };

}}
