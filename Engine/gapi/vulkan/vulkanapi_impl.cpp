#if defined(TEMPEST_BUILD_VULKAN)

#include "vulkanapi_impl.h"

#include <Tempest/Log>
#include <Tempest/Platform>

#include "exceptions/exception.h"
#include "vdevice.h"

#include <cstring>
#include <vector>

#define VK_KHR_WIN32_SURFACE_EXTENSION_NAME "VK_KHR_win32_surface"
#define VK_KHR_XLIB_SURFACE_EXTENSION_NAME  "VK_KHR_xlib_surface"

#if defined(__WINDOWS__)
#define SURFACE_EXTENSION_NAME VK_KHR_WIN32_SURFACE_EXTENSION_NAME
#elif defined(__UNIX__)
#define SURFACE_EXTENSION_NAME VK_KHR_XLIB_SURFACE_EXTENSION_NAME
#endif

using namespace Tempest::Detail;

static const std::initializer_list<const char*> validationLayersKHR = {
  "VK_LAYER_KHRONOS_validation"
  };

static const std::initializer_list<const char*> validationLayersLunarg = {
  "VK_LAYER_LUNARG_core_validation"
  };

static bool checkForExt(const std::vector<VkExtensionProperties>& list, const char* name) {
  for(auto& r:list)
    if(std::strcmp(name,r.extensionName)==0)
      return true;
  return false;
  }


VulkanInstance::VulkanInstance(bool validation)
  :validation(validation) {
  std::initializer_list<const char*> validationLayers={};
  if(validation) {
    validationLayers = checkValidationLayerSupport();
    if(validationLayers.size()==0)
      Log::d("VulkanApi: no validation layers available");
    }

  VkApplicationInfo appInfo = {};
  appInfo.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  //appInfo.pApplicationName   = "";
  appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
  appInfo.pEngineName        = "Tempest";
  appInfo.engineVersion      = VK_MAKE_VERSION(1, 0, 0);
  appInfo.apiVersion         = VK_API_VERSION_1_0;

  if(auto vkEnumerateInstanceVersion = PFN_vkEnumerateInstanceVersion(vkGetInstanceProcAddr(nullptr,"vkEnumerateInstanceVersion"))) {
    (void)vkEnumerateInstanceVersion;
    appInfo.apiVersion = VK_API_VERSION_1_3;
    }

  VkInstanceCreateInfo createInfo = {};
  createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  createInfo.pApplicationInfo = &appInfo;

  std::vector<const char*> rqExt = {
    VK_EXT_DEBUG_REPORT_EXTENSION_NAME,
    VK_KHR_SURFACE_EXTENSION_NAME,
    SURFACE_EXTENSION_NAME,
    };

  auto ext = instExtensionsList();
  if(checkForExt(ext,VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME)) {
    rqExt.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
    hasDeviceFeatures2 = true;
    }

  createInfo.enabledExtensionCount   = uint32_t(rqExt.size());
  createInfo.ppEnabledExtensionNames = rqExt.data();

  if(validation) {
    createInfo.enabledLayerCount   = uint32_t(validationLayers.size());
    createInfo.ppEnabledLayerNames = validationLayers.begin();
    } else {
    createInfo.enabledLayerCount   = 0;
    }

  VkResult ret = vkCreateInstance(&createInfo,nullptr,&instance);
  if(ret!=VK_SUCCESS)
    throw std::system_error(Tempest::GraphicsErrc::NoDevice);

  if(validation) {
    auto vkCreateDebugReportCallbackEXT = PFN_vkCreateDebugReportCallbackEXT (vkGetInstanceProcAddr(instance,"vkCreateDebugReportCallbackEXT"));
    vkDestroyDebugReportCallbackEXT     = PFN_vkDestroyDebugReportCallbackEXT(vkGetInstanceProcAddr(instance,"vkDestroyDebugReportCallbackEXT"));

    /* Setup callback creation information */
    VkDebugReportCallbackCreateInfoEXT callbackCreateInfo;
    callbackCreateInfo.sType       = VK_STRUCTURE_TYPE_DEBUG_REPORT_CREATE_INFO_EXT;
    callbackCreateInfo.pNext       = nullptr;
    callbackCreateInfo.flags       = VK_DEBUG_REPORT_ERROR_BIT_EXT |
                                     VK_DEBUG_REPORT_WARNING_BIT_EXT |
                                     VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT;
    callbackCreateInfo.pfnCallback = &debugReportCallback;
    callbackCreateInfo.pUserData   = nullptr;

    /* Register the callback */
    VkResult result = vkCreateDebugReportCallbackEXT(instance, &callbackCreateInfo, nullptr, &callback);
    if(result!=VK_SUCCESS)
      Log::e("unable to setup validation callback");
    }
  }

VulkanInstance::~VulkanInstance(){
  if(vkDestroyDebugReportCallbackEXT)
    vkDestroyDebugReportCallbackEXT(instance,callback,nullptr);
  vkDestroyInstance(instance,nullptr);
  }

std::vector<VkExtensionProperties> VulkanInstance::instExtensionsList() const {
  uint32_t extensionCount;
  vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);

  std::vector<VkExtensionProperties> ext(extensionCount);
  vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, ext.data());

  return ext;
  }

const std::initializer_list<const char*>& VulkanInstance::checkValidationLayerSupport() {
  uint32_t layerCount = 0;
  vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

  std::vector<VkLayerProperties> availableLayers(layerCount);
  vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

  if(layerSupport(availableLayers,validationLayersKHR))
    return validationLayersKHR;

  if(layerSupport(availableLayers,validationLayersLunarg))
    return validationLayersLunarg;

  static const std::initializer_list<const char*> empty;
  return empty;
  }

bool VulkanInstance::layerSupport(const std::vector<VkLayerProperties>& sup,
                                 const std::initializer_list<const char*> dest) {
  for(auto& i:dest) {
    bool found=false;
    for(auto& r:sup)
      if(std::strcmp(r.layerName,i)==0) {
        found = true;
        break;
        }
    if(!found)
      return false;
    }
  return true;
  }

std::vector<Tempest::AbstractGraphicsApi::Props> VulkanInstance::devices() const {
  std::vector<Tempest::AbstractGraphicsApi::Props> devList;
  uint32_t deviceCount = 0;
  vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

  if(deviceCount==0)
    return devList;

  std::vector<VkPhysicalDevice> devices(deviceCount);
  vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

  devList.resize(devices.size());
  for(size_t i=0; i<devList.size(); ++i) {
    VDevice::VkProps prop = {};
    VDevice::devicePropsShort(*this, devices[i], prop);
    devList[i] = static_cast<Tempest::AbstractGraphicsApi::Props&>(prop);
    }
  return devList;
  }

VkBool32 VulkanInstance::debugReportCallback(VkDebugReportFlagsEXT      flags,
                                             VkDebugReportObjectTypeEXT objectType,
                                             uint64_t                   object,
                                             size_t                     location,
                                             int32_t                    messageCode,
                                             const char                *pLayerPrefix,
                                             const char                *pMessage,
                                             void                      *pUserData) {
  if(flags==VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT && messageCode==0) {
    // [ UNASSIGNED-CoreValidation-DrawState-InvalidImageLayout ]
    // warning: layout should be VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL instead of GENERAL
    return VK_FALSE;
    }
  if(location==461143016) {
    // bug?
    return VK_FALSE;
    }
  if(location==3578306442) {
    // The Vulkan spec states: If range is not equal to VK_WHOLE_SIZE, range must be greater than 0
    // can't workaround this without VK_EXT_robustness2
    return VK_FALSE;
    }

  Log::e(pMessage," object=",object,", type=",objectType," th:",std::this_thread::get_id());
  return VK_FALSE;
  }

#endif
