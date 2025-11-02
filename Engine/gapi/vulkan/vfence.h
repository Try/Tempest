#pragma once

#include <Tempest/AbstractGraphicsApi>
#include "vulkan_sdk.h"

namespace Tempest {
namespace Detail {

class VDevice;

struct VFence : public AbstractGraphicsApi::Fence {
  VFence(VDevice* device, VkFence f, uint32_t id):device(device), fence(f), id(id) {}

  void wait() override;
  bool wait(uint64_t time) override;

  VDevice* device = nullptr;
  VkFence  fence  = VK_NULL_HANDLE;
  uint32_t id     = 0;
  VkResult status = VK_SUCCESS;
  };

}}
