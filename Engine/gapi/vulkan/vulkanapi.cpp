#include "vulkanapi.h"

#include <set>
#define VK_KHR_WIN32_SURFACE_EXTENSION_NAME "VK_KHR_win32_surface"

using namespace Tempest::Detail;

static const std::initializer_list<const char*> validationLayers = {
  "VK_LAYER_LUNARG_standard_validation"
  };

static const std::vector<const char*> deviceExtensions = {
  VK_KHR_SWAPCHAIN_EXTENSION_NAME
  };

VulkanApi::VulkanApi(bool validation)
  :validation(validation){
  /*
  if(enableValidationLayers && !checkValidationLayerSupport())
    throw std::runtime_error("validation layers requested, but not available!");*/

  VkApplicationInfo appInfo = {};
  appInfo.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  //appInfo.pApplicationName   = "Hello Triangle";
  appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
  appInfo.pEngineName        = "Tempest";
  appInfo.engineVersion      = VK_MAKE_VERSION(1, 0, 0);
  appInfo.apiVersion         = VK_API_VERSION_1_0;

  VkInstanceCreateInfo createInfo = {};
  createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  createInfo.pApplicationInfo = &appInfo;

  auto extensions = std::initializer_list<const char*>{
    VK_EXT_DEBUG_REPORT_EXTENSION_NAME,
    VK_KHR_SURFACE_EXTENSION_NAME,
    VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
    };

  createInfo.enabledExtensionCount   = static_cast<uint32_t>(extensions.size());
  createInfo.ppEnabledExtensionNames = extensions.begin();

  if( validation ){
    createInfo.enabledLayerCount   = static_cast<uint32_t>(validationLayers.size());
    createInfo.ppEnabledLayerNames = validationLayers.begin();
    } else {
    createInfo.enabledLayerCount = 0;
    }

  if(vkCreateInstance(&createInfo,nullptr,&instance)!=VK_SUCCESS)
    throw std::runtime_error("failed to create instance!");
  }

VulkanApi::~VulkanApi(){
  vkDestroyInstance(instance,nullptr);
  }
