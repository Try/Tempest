#include "vulkanapi_impl.h"

#include <Tempest/Log>
#include <Tempest/Platform>

#include "exceptions/exception.h"
#include "vdevice.h"

#include <set>
#include <thread>
#include <cstring>

#define VK_KHR_WIN32_SURFACE_EXTENSION_NAME "VK_KHR_win32_surface"
#define VK_KHR_XLIB_SURFACE_EXTENSION_NAME  "VK_KHR_xlib_surface"

#if defined(__WINDOWS__)
#define SURFACE_EXTENSION_NAME VK_KHR_WIN32_SURFACE_EXTENSION_NAME
#elif defined(__LINUX__)
#define SURFACE_EXTENSION_NAME VK_KHR_XLIB_SURFACE_EXTENSION_NAME
#endif

using namespace Tempest::Detail;

static const std::initializer_list<const char*> validationLayersKHR = {
  "VK_LAYER_KHRONOS_validation"
  };

static const std::initializer_list<const char*> validationLayersLunarg = {
  "VK_LAYER_LUNARG_core_validation"
  };

VulkanApi::VulkanApi(bool validation)
  :validation(validation) {
  std::initializer_list<const char*> validationLayers={};
  if(validation) {
    validationLayers = checkValidationLayerSupport();
    if(validationLayers.size()==0)
      Log::d("VulkanApi: no validation layers available");
    }

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
    SURFACE_EXTENSION_NAME,
    };

  createInfo.enabledExtensionCount   = static_cast<uint32_t>(extensions.size());
  createInfo.ppEnabledExtensionNames = extensions.begin();

  if(validation){
    createInfo.enabledLayerCount   = static_cast<uint32_t>(validationLayers.size());
    createInfo.ppEnabledLayerNames = validationLayers.begin();
    } else {
    createInfo.enabledLayerCount = 0;
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

VulkanApi::~VulkanApi(){
  if( vkDestroyDebugReportCallbackEXT )
    vkDestroyDebugReportCallbackEXT(instance,callback,nullptr);
  vkDestroyInstance(instance,nullptr);
  }

const std::initializer_list<const char*> VulkanApi::checkValidationLayerSupport() {
  uint32_t layerCount=0;
  vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

  std::vector<VkLayerProperties> availableLayers(layerCount);
  vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

  if(layerSupport(availableLayers,validationLayersKHR))
    return validationLayersKHR;

  if(layerSupport(availableLayers,validationLayersLunarg))
    return validationLayersLunarg;

  return {};
  }

bool VulkanApi::layerSupport(const std::vector<VkLayerProperties>& sup,
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

std::vector<Tempest::AbstractGraphicsApi::Props> VulkanApi::devices() const {
  std::vector<Tempest::AbstractGraphicsApi::Props> devList;
  uint32_t deviceCount = 0;
  vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

  if(deviceCount==0)
    return devList;

  std::vector<VkPhysicalDevice> devices(deviceCount);
  vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

  devList.resize(devices.size());
  for(size_t i=0;i<devList.size();++i) {
    getDevicePropsShort(devices[i],devList[i]);
    }
  return devList;
  }

void VulkanApi::getDeviceProps(VkPhysicalDevice physicalDevice, VkProp& c) {
  getDevicePropsShort(physicalDevice,c);

  VkPhysicalDeviceProperties prop={};
  vkGetPhysicalDeviceProperties(physicalDevice,&prop);
  c.nonCoherentAtomSize = size_t(prop.limits.nonCoherentAtomSize);
  if(c.nonCoherentAtomSize==0)
    c.nonCoherentAtomSize=1;

  c.bufferImageGranularity = size_t(prop.limits.bufferImageGranularity);
  if(c.bufferImageGranularity==0)
    c.bufferImageGranularity=1;
  }

void VulkanApi::getDevicePropsShort(VkPhysicalDevice physicalDevice, Tempest::AbstractGraphicsApi::Props& c) {
  /*
   * formats support table: https://vulkan.lunarg.com/doc/view/1.0.30.0/linux/vkspec.chunked/ch31s03.html
   */
  VkFormatFeatureFlags imageRqFlags    = VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT|VK_FORMAT_FEATURE_BLIT_DST_BIT|VK_FORMAT_FEATURE_BLIT_SRC_BIT;
  VkFormatFeatureFlags imageRqFlagsBC  = VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;
  VkFormatFeatureFlags attachRqFlags   = VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT|VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT;
  VkFormatFeatureFlags depthAttflags   = VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;
  VkFormatFeatureFlags storageAttflags = VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT;

  VkPhysicalDeviceProperties prop={};
  vkGetPhysicalDeviceProperties(physicalDevice,&prop);

  VkPhysicalDeviceFeatures supportedFeatures={};
  vkGetPhysicalDeviceFeatures(physicalDevice,&supportedFeatures);

  std::memcpy(c.name,prop.deviceName,sizeof(c.name));

  c.vbo.maxAttribs  = size_t(prop.limits.maxVertexInputAttributes);
  c.vbo.maxRange    = size_t(prop.limits.maxVertexInputBindingStride);

  c.ibo.maxValue    = size_t(prop.limits.maxDrawIndexedIndexValue);

  c.ubo.offsetAlign = size_t(prop.limits.minUniformBufferOffsetAlignment);
  c.ubo.maxRange    = size_t(prop.limits.maxUniformBufferRange);
  
  c.push.maxRange   = size_t(prop.limits.maxPushConstantsSize);

  c.anisotropy      = supportedFeatures.samplerAnisotropy;
  c.maxAnisotropy   = prop.limits.maxSamplerAnisotropy;

  c.mrt.maxColorAttachments = prop.limits.maxColorAttachments;

  switch(prop.deviceType) {
    case VK_PHYSICAL_DEVICE_TYPE_CPU:
      c.type = AbstractGraphicsApi::DeviceType::Cpu;
      break;
    case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
      c.type = AbstractGraphicsApi::DeviceType::Virtual;
      break;
    case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
      c.type = AbstractGraphicsApi::DeviceType::Integrated;
      break;
    case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
      c.type = AbstractGraphicsApi::DeviceType::Discrete;
      break;
    default:
      c.type = AbstractGraphicsApi::DeviceType::Unknown;
      break;
    }

  uint64_t smpFormat=0, attFormat=0, dattFormat=0, storFormat=0;
  for(uint32_t i=0;i<TextureFormat::Last;++i){
    VkFormat f = Detail::nativeFormat(TextureFormat(i));

    VkFormatProperties frm={};
    vkGetPhysicalDeviceFormatProperties(physicalDevice,f,&frm);
    if(isCompressedFormat(TextureFormat(i))){
      if((frm.optimalTilingFeatures & imageRqFlagsBC)==imageRqFlagsBC){
        smpFormat |= (1ull<<i);
        }
      } else {
      if((frm.optimalTilingFeatures & imageRqFlags)==imageRqFlags){
        smpFormat |= (1ull<<i);
        }
      }
    if((frm.optimalTilingFeatures & attachRqFlags)==attachRqFlags){
      attFormat |= (1ull<<i);
      }
    if((frm.optimalTilingFeatures & depthAttflags)==depthAttflags){
      dattFormat |= (1ull<<i);
      }
    if((frm.optimalTilingFeatures & storageAttflags)==storageAttflags){
      storFormat |= (1ull<<i);
      }
    }
  c.setSamplerFormats(smpFormat);
  c.setAttachFormats (attFormat);
  c.setDepthFormats  (dattFormat);
  c.setStorageFormats(storFormat);
  }

VkBool32 VulkanApi::debugReportCallback(VkDebugReportFlagsEXT      flags,
                                        VkDebugReportObjectTypeEXT objectType,
                                        uint64_t                   object,
                                        size_t                     location,
                                        int32_t                    messageCode,
                                        const char                *pLayerPrefix,
                                        const char                *pMessage,
                                        void                      *pUserData) {
#if VK_HEADER_VERSION==135
  // https://github.com/KhronosGroup/Vulkan-ValidationLayers/issues/1712
  if(objectType==VK_DEBUG_REPORT_OBJECT_TYPE_QUEUE_EXT)
    return VK_FALSE;
#endif
  Log::e(pMessage," object=",object,", type=",objectType," th:",std::this_thread::get_id());
  return VK_FALSE;
  }
