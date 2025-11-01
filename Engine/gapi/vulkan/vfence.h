#pragma once

#include <Tempest/AbstractGraphicsApi>
#include "vulkan_sdk.h"

namespace Tempest {
namespace Detail {

class VDevice;

struct VTimepoint {
  VTimepoint(VkFence f, uint32_t id):fence(f), id(id) {}

  VkFence  fence  = VK_NULL_HANDLE;
  uint32_t id     = 0;
  VkResult status = VK_SUCCESS;
  };

class VFence : public AbstractGraphicsApi::Fence {
  public:
    VFence(VDevice& dev);
    ~VFence() override;

    void wait() override;
    bool wait(uint64_t time) override;
    void reset();

    std::weak_ptr<VTimepoint> timepoint;
    VDevice*                  device = nullptr;
  };

}}
