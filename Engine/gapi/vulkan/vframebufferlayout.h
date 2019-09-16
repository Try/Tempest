#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <vulkan/vulkan.hpp>

#include "vrenderpass.h"

namespace Tempest {
namespace Detail {

class VSwapchain;

class VFramebufferLayout : public AbstractGraphicsApi::FboLayout {
  public:
    VFramebufferLayout(VDevice &dev, VSwapchain& sw, const VkFormat* attach, uint8_t attCount);

    VRenderPass rp;
  };

}}
