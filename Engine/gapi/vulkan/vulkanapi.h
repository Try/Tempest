#pragma once

#include <vulkan/vulkan.hpp>

namespace Tempest {
namespace Detail {

class VulkanApi {
  public:
    VulkanApi(bool enableValidationLayers=true);
    ~VulkanApi();

    const bool       validation;
    VkInstance       instance;
    //VkDebugUtilsMessengerEXT callback;

  private:
  };

}}
