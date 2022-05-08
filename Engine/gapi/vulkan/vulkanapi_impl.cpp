#if defined(TEMPEST_BUILD_VULKAN)

#include "vulkanapi_impl.h"

#include <Tempest/Log>
#include <Tempest/Platform>

#include "exceptions/exception.h"
#include "utility/smallarray.h"
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
    appInfo.apiVersion = VK_API_VERSION_1_2;
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

  createInfo.enabledExtensionCount   = static_cast<uint32_t>(rqExt.size());
  createInfo.ppEnabledExtensionNames = rqExt.data();

  if(validation) {
    createInfo.enabledLayerCount   = static_cast<uint32_t>(validationLayers.size());
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

std::vector<VkExtensionProperties> VulkanInstance::extensionsList(VkPhysicalDevice dev) {
  uint32_t extensionCount;
  vkEnumerateDeviceExtensionProperties(dev, nullptr, &extensionCount, nullptr);

  std::vector<VkExtensionProperties> ext(extensionCount);
  vkEnumerateDeviceExtensionProperties(dev, nullptr, &extensionCount, ext.data());

  return ext;
  }

const std::initializer_list<const char*>& VulkanInstance::checkValidationLayerSupport() {
  uint32_t layerCount=0;
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
  for(size_t i=0;i<devList.size();++i) {
    devicePropsShort(devices[i],devList[i]);
    }
  return devList;
  }

bool VulkanInstance::checkForExt(const std::vector<VkExtensionProperties>& list, const char* name) {
  for(auto& r:list)
    if(std::strcmp(name,r.extensionName)==0)
      return true;
  return false;
  }

void VulkanInstance::deviceProps(VkPhysicalDevice physicalDevice, VkProp& c) const {
  devicePropsShort(physicalDevice,c);

  VkPhysicalDeviceProperties prop={};
  vkGetPhysicalDeviceProperties(physicalDevice,&prop);
  c.nonCoherentAtomSize = size_t(prop.limits.nonCoherentAtomSize);
  if(c.nonCoherentAtomSize==0)
    c.nonCoherentAtomSize=1;

  c.bufferImageGranularity = size_t(prop.limits.bufferImageGranularity);
  if(c.bufferImageGranularity==0)
    c.bufferImageGranularity=1;
  }

void VulkanInstance::devicePropsShort(VkPhysicalDevice physicalDevice, Tempest::AbstractGraphicsApi::Props& c) const {
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

  if(hasDeviceFeatures2) {
    VkPhysicalDeviceFeatures2 features = {};
    features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2_KHR;

    VkPhysicalDeviceRayQueryFeaturesKHR rayQueryFeatures = {};
    rayQueryFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR;

    VkPhysicalDeviceMeshShaderFeaturesNV meshFeatures = {};
    meshFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_NV;

    VkPhysicalDeviceDescriptorIndexingFeatures indexingFeatures = {};
    indexingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;

    auto ext = extensionsList(physicalDevice);
    if(checkForExt(ext,VK_KHR_RAY_QUERY_EXTENSION_NAME)) {
      rayQueryFeatures.pNext = features.pNext;
      features.pNext = &rayQueryFeatures;
      }
    if(checkForExt(ext,VK_NV_MESH_SHADER_EXTENSION_NAME)) {
      meshFeatures.pNext = features.pNext;
      features.pNext = &meshFeatures;
      }
    if(checkForExt(ext,VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME)) {
      indexingFeatures.pNext = features.pNext;
      features.pNext = &indexingFeatures;
      }
    auto vkGetPhysicalDeviceFeatures2 = PFN_vkGetPhysicalDeviceFeatures2(vkGetInstanceProcAddr(instance,"vkGetPhysicalDeviceFeatures2"));
    vkGetPhysicalDeviceFeatures2(physicalDevice,&features);
    c.raytracing.rayQuery = rayQueryFeatures.rayQuery!=VK_FALSE;
    c.meshShader          = meshFeatures.meshShader!=VK_FALSE && meshFeatures.taskShader!=VK_FALSE;
    if(indexingFeatures.runtimeDescriptorArray!=VK_FALSE) {
      c.bindless.sampledImage = (indexingFeatures.shaderSampledImageArrayNonUniformIndexing==VK_TRUE);
      }
    }

  std::memcpy(c.name,prop.deviceName,sizeof(c.name));

  c.vbo.maxAttribs    = size_t(prop.limits.maxVertexInputAttributes);
  c.vbo.maxRange      = size_t(prop.limits.maxVertexInputBindingStride);

  c.ibo.maxValue      = size_t(prop.limits.maxDrawIndexedIndexValue);

  c.ssbo.offsetAlign  = size_t(prop.limits.minUniformBufferOffsetAlignment);
  c.ssbo.maxRange     = size_t(prop.limits.maxStorageBufferRange);

  c.ubo.offsetAlign   = size_t(prop.limits.minUniformBufferOffsetAlignment);
  c.ubo.maxRange      = size_t(prop.limits.maxUniformBufferRange);
  
  c.push.maxRange     = size_t(prop.limits.maxPushConstantsSize);

  c.anisotropy        = supportedFeatures.samplerAnisotropy;
  c.maxAnisotropy     = prop.limits.maxSamplerAnisotropy;
  c.tesselationShader = supportedFeatures.tessellationShader;
  c.geometryShader    = supportedFeatures.geometryShader;

  c.storeAndAtomicVs  = supportedFeatures.vertexPipelineStoresAndAtomics;
  c.storeAndAtomicFs  = supportedFeatures.fragmentStoresAndAtomics;

  c.mrt.maxColorAttachments = prop.limits.maxColorAttachments;

  c.compute.maxGroups.x = prop.limits.maxComputeWorkGroupCount[0];
  c.compute.maxGroups.y = prop.limits.maxComputeWorkGroupCount[1];
  c.compute.maxGroups.z = prop.limits.maxComputeWorkGroupCount[2];

  c.compute.maxGroupSize.x = prop.limits.maxComputeWorkGroupSize[0];
  c.compute.maxGroupSize.y = prop.limits.maxComputeWorkGroupSize[1];
  c.compute.maxGroupSize.z = prop.limits.maxComputeWorkGroupSize[2];

  c.tex2d.maxSize = prop.limits.maxImageDimension2D;

  switch(prop.deviceType) {
    case VK_PHYSICAL_DEVICE_TYPE_CPU:
      c.type = DeviceType::Cpu;
      break;
    case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
      c.type = DeviceType::Virtual;
      break;
    case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
      c.type = DeviceType::Integrated;
      break;
    case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
      c.type = DeviceType::Discrete;
      break;
    default:
      c.type = DeviceType::Unknown;
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

void VulkanInstance::submit(VDevice* dev, VCommandBuffer** cmd, size_t count, VFence* fence) {
  size_t waitCnt = 0;
  for(size_t i=0; i<count; ++i) {
    for(auto& s:cmd[i]->swapchainSync) {
      if(s->state!=Detail::VSwapchain::S_Pending)
        continue;
      s->state = Detail::VSwapchain::S_Draw0;
      ++waitCnt;
      }
    }

  SmallArray<VkCommandBuffer, 32>      command(count);
  SmallArray<VkSemaphore, 32>          wait      (waitCnt);
  SmallArray<VkPipelineStageFlags, 32> waitStages(waitCnt);

  size_t waitId = 0;
  for(size_t i=0; i<count; ++i) {
    command[i] = cmd[i]->impl;
    for(auto& s:cmd[i]->swapchainSync) {
      if(s->state!=Detail::VSwapchain::S_Draw0)
        continue;
      s->state = Detail::VSwapchain::S_Draw1;
      wait[waitId] = s->aquire;
      ++waitId;
      }
    }

  for(size_t i=0; i<waitCnt; ++i) {
    // NOTE: our sw images are draw-only
    waitStages[i] = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    }

  VkSubmitInfo submitInfo = {};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

  submitInfo.waitSemaphoreCount   = uint32_t(waitCnt);
  submitInfo.pWaitSemaphores      = wait.get();
  submitInfo.pWaitDstStageMask    = waitStages.get();

  submitInfo.commandBufferCount   = uint32_t(count);
  submitInfo.pCommandBuffers      = command.get();

  submitInfo.signalSemaphoreCount = 0;
  submitInfo.pSignalSemaphores    = nullptr;

  if(fence!=nullptr)
    fence->reset();

  dev->dataMgr().wait();
  dev->graphicsQueue->submit(1, &submitInfo, fence==nullptr ? VK_NULL_HANDLE : fence->impl);
  }

VkBool32 VulkanInstance::debugReportCallback(VkDebugReportFlagsEXT      flags,
                                            VkDebugReportObjectTypeEXT objectType,
                                            uint64_t                   object,
                                            size_t                     location,
                                            int32_t                    messageCode,
                                            const char                *pLayerPrefix,
                                            const char                *pMessage,
                                            void                      *pUserData) {
  Log::e(pMessage," object=",object,", type=",objectType," th:",std::this_thread::get_id());
  return VK_FALSE;
  }

#endif
