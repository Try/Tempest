#if defined(TEMPEST_BUILD_VULKAN)

#include "vdevice.h"

#include "vcommandbuffer.h"
#include "vfence.h"
#include "vswapchain.h"
#include "system/api/x11api.h"

#include <Tempest/Log>
#include <Tempest/Platform>
#include <cstring>
#include <array>

#if defined(__WINDOWS__)
#  define VK_USE_PLATFORM_WIN32_KHR
#  include <windows.h>
#  include <vulkan/vulkan_win32.h>
#elif defined(__UNIX__)
#  define VK_USE_PLATFORM_XLIB_KHR
#  include <X11/Xlib.h>
#  include <vulkan/vulkan_xlib.h>
#  undef Always
#  undef None
#else
#  error "WSI is not implemented on this platform"
#endif

using namespace Tempest;
using namespace Tempest::Detail;

static const std::initializer_list<const char*> requiredExtensions = {
  VK_KHR_SWAPCHAIN_EXTENSION_NAME
  };

VDevice::autoDevice::~autoDevice() {
  vkDestroyDevice(impl,nullptr);
  }


VDevice::VDevice(VulkanInstance &api, std::string_view gpuName)
  :instance(api.instance), fboMap(*this), setLayouts(*this), psoLayouts(*this), descPool(*this), bindless(*this) {
  uint32_t deviceCount = 0;
  vkEnumeratePhysicalDevices(api.instance, &deviceCount, nullptr);

  if(deviceCount==0)
    throw std::system_error(Tempest::GraphicsErrc::NoDevice);

  std::vector<VkPhysicalDevice> devices(deviceCount);
  vkEnumeratePhysicalDevices(api.instance, &deviceCount, devices.data());

  for(const auto& device:devices) {
    if(isDeviceSuitable(device,gpuName)) {
      implInit(api,device);
      return;
      }
    }

  throw std::system_error(Tempest::GraphicsErrc::NoDevice);
  }

VDevice::~VDevice(){
  vkDeviceWaitIdle(device.impl);
  data.reset();
  }

void VDevice::implInit(VulkanInstance &api, VkPhysicalDevice pdev) {
  api.deviceProps(pdev,props);
  deviceQueueProps(props,pdev);

  createLogicalDevice(api,pdev);
  vkGetPhysicalDeviceMemoryProperties(pdev,&memoryProperties);

  physicalDevice = pdev;
  allocator.setDevice(*this);
  descPool.setupLimits(api);
  data.reset(new DataMgr(*this));
  }

VkSurfaceKHR VDevice::createSurface(void* hwnd) {
  if(hwnd==nullptr)
    return VK_NULL_HANDLE;
  VkSurfaceKHR ret = VK_NULL_HANDLE;
#ifdef __WINDOWS__
  VkWin32SurfaceCreateInfoKHR createInfo={};
  createInfo.sType     = VkStructureType::VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
  createInfo.hinstance = GetModuleHandleA(nullptr);
  createInfo.hwnd      = HWND(hwnd);
  if(vkCreateWin32SurfaceKHR(instance,&createInfo,nullptr,&ret)!=VK_SUCCESS)
    throw std::system_error(Tempest::GraphicsErrc::NoDevice);
#elif defined(__UNIX__)
  VkXlibSurfaceCreateInfoKHR createInfo = {};
  createInfo.sType  = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR;
  createInfo.dpy    = reinterpret_cast<Display*>(X11Api::display());
  createInfo.window = ::Window(hwnd);
  if(vkCreateXlibSurfaceKHR(instance, &createInfo, nullptr, &ret)!=VK_SUCCESS)
    throw std::system_error(Tempest::GraphicsErrc::NoDevice);
#else
#warning "wsi for vulkan not implemented on this platform"
#endif
  return ret;
  }

bool VDevice::isDeviceSuitable(VkPhysicalDevice device, std::string_view gpuName) {
  if(!gpuName.empty()) {
    VkPhysicalDeviceProperties prop={};
    vkGetPhysicalDeviceProperties(device,&prop);
    if(gpuName!=prop.deviceName)
      return false;
    }
  VkProps prop = {};
  deviceQueueProps(prop,device);
  bool extensionsSupported = checkDeviceExtensionSupport(device);
  return extensionsSupported && prop.graphicsFamily!=uint32_t(-1);
  }

void VDevice::deviceQueueProps(VulkanInstance::VkProp& prop, VkPhysicalDevice device) {
  uint32_t queueFamilyCount = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

  std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
  vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

  uint32_t graphics  = uint32_t(-1);
  uint32_t present   = uint32_t(-1);
  uint32_t universal = uint32_t(-1);

  for(uint32_t i=0;i<queueFamilyCount;++i) {
    const auto& queueFamily = queueFamilies[i];
    if(queueFamily.queueCount<=0)
      continue;

    static const VkQueueFlags rqFlag = (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT);

    const bool graphicsSupport = ((queueFamily.queueFlags & rqFlag)==rqFlag);
#if defined(__WINDOWS__)
    const bool presentSupport = vkGetPhysicalDeviceWin32PresentationSupportKHR(device,i)!=VK_FALSE;
#elif defined(__UNIX__)
    bool presentSupport = false;
    if(auto dpy = reinterpret_cast<Display*>(X11Api::display())){
      auto screen   = DefaultScreen(dpy);
      auto visualId = XVisualIDFromVisual(DefaultVisual(dpy,screen));
      presentSupport = vkGetPhysicalDeviceXlibPresentationSupportKHR(device,i,dpy,visualId)!=VK_FALSE;
      }
#else
#warning "wsi for vulkan not implemented on this platform"
#endif

    if(graphicsSupport)
      graphics = i;
    if(presentSupport)
      present = i;
    if(presentSupport && graphicsSupport)
      universal = i;
    }

  if(universal!=uint32_t(-1)) {
    graphics = universal;
    present  = universal;
    }

  prop.graphicsFamily = graphics;
  prop.presentFamily  = present;
  }

bool VDevice::checkDeviceExtensionSupport(VkPhysicalDevice device) {
  auto ext = extensionsList(device);

  for(auto& i:requiredExtensions) {
    if(!checkForExt(ext,i))
      return false;
    }

  return true;
  }

std::vector<VkExtensionProperties> VDevice::extensionsList(VkPhysicalDevice device) {
  return VulkanInstance::extensionsList(device);
  }

bool VDevice::checkForExt(const std::vector<VkExtensionProperties>& list, const char* name) {
  return VulkanInstance::checkForExt(list,name);
  }

template<class R, class ... Args>
void dummyIfNull(R (*&fn)(Args...a)) {
  struct Dummy {
    static R fn(Args...a) { return R(); }
    };

  if(fn==nullptr)
    fn = Dummy::fn;
  }

VDevice::SwapChainSupport VDevice::querySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface) {
  SwapChainSupport details;
  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

  uint32_t formatCount = 0;
  vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);
  details.formats.resize(formatCount);
  vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());

  uint32_t presentModeCount = 0;
  vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);
  details.presentModes.resize(presentModeCount);
  vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());

  return details;
  }

void VDevice::createLogicalDevice(VulkanInstance &api, VkPhysicalDevice pdev) {
  std::array<uint32_t,2>  uniqueQueueFamilies = {props.graphicsFamily, props.presentFamily};
  float                   queuePriority       = 1.0f;
  size_t                  queueCnt            = 0;
  VkDeviceQueueCreateInfo qinfo[3]={};
  for(size_t i=0;i<uniqueQueueFamilies.size();++i) {
    auto&    q      = queues[queueCnt];
    uint32_t family = uniqueQueueFamilies[i];
    if(family==uint32_t(-1))
      continue;

    bool nonUnique=false;
    for(size_t r=0;r<queueCnt;++r)
      if(q.family==family)
        nonUnique = true;
    if(nonUnique)
      continue;

    VkDeviceQueueCreateInfo& queueCreateInfo = qinfo[queueCnt];
    queueCreateInfo.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = family;
    queueCreateInfo.queueCount       = 1;
    queueCreateInfo.pQueuePriorities = &queuePriority;

    q.family = family;
    queueCnt++;
    }

  std::vector<const char*> rqExt = requiredExtensions;
  if(props.hasMemRq2) {
    rqExt.push_back(VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME);
    }
  if(props.hasDedicatedAlloc) {
    rqExt.push_back(VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME);
    }
  if(props.hasSync2) {
    rqExt.push_back(VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME);
    }
  if(props.hasDeviceAddress) {
    rqExt.push_back(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME);
    }
  if(props.hasSpirv_1_4) {
    rqExt.push_back(VK_KHR_SPIRV_1_4_EXTENSION_NAME);
    }
  if(props.raytracing.rayQuery) {
    rqExt.push_back(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
    rqExt.push_back(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
    rqExt.push_back(VK_KHR_RAY_QUERY_EXTENSION_NAME);
    }
  if(props.meshlets.meshShader) {
    rqExt.push_back(VK_EXT_MESH_SHADER_EXTENSION_NAME);
    }
  if(props.hasDescIndexing) {
    rqExt.push_back(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME);
    }
  if(props.hasDynRendering) {
    rqExt.push_back(VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME);
    }
  if(props.hasBarycentrics) {
    rqExt.push_back(VK_KHR_FRAGMENT_SHADER_BARYCENTRIC_EXTENSION_NAME);
    }
  if(props.hasRobustness2) {
    rqExt.push_back(VK_EXT_ROBUSTNESS_2_EXTENSION_NAME);
    }
  if(props.hasStoreOpNone) {
    rqExt.push_back(VK_EXT_LOAD_STORE_OP_NONE_EXTENSION_NAME);
    }
  if(props.hasMaintenance1) {
    rqExt.push_back(VK_KHR_MAINTENANCE_1_EXTENSION_NAME);
    //rqExt.push_back(VK_EXT_IMAGE_2D_VIEW_OF_3D_EXTENSION_NAME);
    }
  if(props.hasDebugMarker) {
    rqExt.push_back(VK_EXT_DEBUG_MARKER_EXTENSION_NAME);
    }

  VkPhysicalDeviceFeatures supportedFeatures={};
  vkGetPhysicalDeviceFeatures(pdev,&supportedFeatures);

  VkPhysicalDeviceFeatures deviceFeatures = {};
  deviceFeatures.samplerAnisotropy    = supportedFeatures.samplerAnisotropy;
  deviceFeatures.textureCompressionBC = supportedFeatures.textureCompressionBC;
  deviceFeatures.tessellationShader   = supportedFeatures.tessellationShader;
  deviceFeatures.geometryShader       = supportedFeatures.geometryShader;
  deviceFeatures.fillModeNonSolid     = supportedFeatures.fillModeNonSolid;

  deviceFeatures.vertexPipelineStoresAndAtomics = supportedFeatures.vertexPipelineStoresAndAtomics;
  deviceFeatures.fragmentStoresAndAtomics       = supportedFeatures.fragmentStoresAndAtomics;

  VkDeviceCreateInfo createInfo = {};
  createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

  createInfo.queueCreateInfoCount = uint32_t(queueCnt);
  createInfo.pQueueCreateInfos    = &qinfo[0];
  createInfo.pEnabledFeatures     = &deviceFeatures;

  createInfo.enabledExtensionCount   = static_cast<uint32_t>(rqExt.size());
  createInfo.ppEnabledExtensionNames = rqExt.data();

  if(api.hasDeviceFeatures2) {
    VkPhysicalDeviceFeatures2 features = {};
    features.sType    = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2_KHR;
    features.features = deviceFeatures;

    VkPhysicalDeviceDynamicRenderingFeaturesKHR dynRendering = {};
    dynRendering.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR;

    VkPhysicalDeviceSynchronization2FeaturesKHR sync2 = {};
    sync2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES_KHR;

    VkPhysicalDeviceBufferDeviceAddressFeaturesKHR bdaFeatures = {};
    bdaFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES_KHR;

    VkPhysicalDeviceAccelerationStructureFeaturesKHR asFeatures = {};
    asFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;

    VkPhysicalDeviceRayQueryFeaturesKHR rayQueryFeatures = {};
    rayQueryFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR;

    VkPhysicalDeviceMeshShaderFeaturesEXT meshFeatures = {};
    meshFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT;

    VkPhysicalDeviceDescriptorIndexingFeatures indexingFeatures = {};
    indexingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;

    VkPhysicalDeviceFragmentShaderBarycentricFeaturesKHR barycentricsFeatures = {};
    barycentricsFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FRAGMENT_SHADER_BARYCENTRIC_FEATURES_KHR;

    VkPhysicalDeviceRobustness2FeaturesEXT robustness2Features = {};
    robustness2Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_FEATURES_EXT;

    VkPhysicalDeviceVulkanMemoryModelFeatures memoryFeatures = {};
    memoryFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_MEMORY_MODEL_FEATURES;

    if(props.hasSync2) {
      sync2.pNext = features.pNext;
      features.pNext = &sync2;
      }
    if(props.hasDeviceAddress) {
      bdaFeatures.pNext = features.pNext;
      features.pNext = &bdaFeatures;
      }
    if(props.raytracing.rayQuery) {
      asFeatures.pNext = features.pNext;
      features.pNext = &asFeatures;
      }
    if(props.raytracing.rayQuery) {
      rayQueryFeatures.pNext = features.pNext;
      features.pNext = &rayQueryFeatures;
      }
    if(props.hasDynRendering) {
      dynRendering.pNext = features.pNext;
      features.pNext = &dynRendering;
      }
    if(props.meshlets.meshShader) {
      meshFeatures.pNext = features.pNext;
      features.pNext = &meshFeatures;
      }
    if(props.hasDescIndexing) {
      indexingFeatures.pNext = features.pNext;
      features.pNext = &indexingFeatures;
      }
    if(props.hasBarycentrics) {
      barycentricsFeatures.pNext = features.pNext;
      features.pNext = &barycentricsFeatures;
      }
    if(props.hasRobustness2) {
      robustness2Features.pNext = features.pNext;
      features.pNext = &robustness2Features;
      }
    if(props.hasStoreOpNone) {
      // no feature bits
      }
    if(props.memoryModel) {
      memoryFeatures.pNext = features.pNext;
      features.pNext = &memoryFeatures;
      }

    auto vkGetPhysicalDeviceFeatures2 = PFN_vkGetPhysicalDeviceFeatures2(vkGetInstanceProcAddr(instance,"vkGetPhysicalDeviceFeatures2KHR"));

    vkGetPhysicalDeviceFeatures2(pdev, &features);
    bdaFeatures.bufferDeviceAddressCaptureReplay  = VK_FALSE;
    bdaFeatures.bufferDeviceAddressMultiDevice    = VK_FALSE;
    asFeatures.accelerationStructureCaptureReplay = VK_FALSE;
    asFeatures.accelerationStructureIndirectBuild = VK_FALSE;
    asFeatures.accelerationStructureHostCommands  = VK_FALSE;
    asFeatures.descriptorBindingAccelerationStructureUpdateAfterBind = VK_FALSE;

    robustness2Features.robustBufferAccess2 = VK_FALSE;
    robustness2Features.robustImageAccess2 = VK_FALSE;

    meshFeatures.multiviewMeshShader                    = VK_FALSE; // requires VkPhysicalDeviceMultiviewFeaturesKHR::multiview
    meshFeatures.primitiveFragmentShadingRateMeshShader = VK_FALSE; // requires vrs

    createInfo.pNext            = &features;
    createInfo.pEnabledFeatures = nullptr;
    if(vkCreateDevice(pdev, &createInfo, nullptr, &device.impl)!=VK_SUCCESS)
      throw std::system_error(Tempest::GraphicsErrc::NoDevice);
    } else {
    if(vkCreateDevice(pdev, &createInfo, nullptr, &device.impl)!=VK_SUCCESS)
      throw std::system_error(Tempest::GraphicsErrc::NoDevice);
    }

  for(size_t i=0; i<queueCnt; ++i) {
    vkGetDeviceQueue(device.impl, queues[i].family, 0, &queues[i].impl);
    if(queues[i].family==props.graphicsFamily)
      graphicsQueue = &queues[i];
    if(queues[i].family==props.presentFamily)
      presentQueue = &queues[i];
    }

  if(props.hasMemRq2) {
    vkGetBufferMemoryRequirements2 = PFN_vkGetBufferMemoryRequirements2KHR(vkGetDeviceProcAddr(device.impl,"vkGetBufferMemoryRequirements2KHR"));
    vkGetImageMemoryRequirements2  = PFN_vkGetImageMemoryRequirements2KHR (vkGetDeviceProcAddr(device.impl,"vkGetImageMemoryRequirements2KHR"));
    }

  if(props.hasSync2) {
    vkCmdPipelineBarrier2 = PFN_vkCmdPipelineBarrier2KHR(vkGetDeviceProcAddr(device.impl,"vkCmdPipelineBarrier2KHR"));
    vkQueueSubmit2        = PFN_vkQueueSubmit2KHR       (vkGetDeviceProcAddr(device.impl,"vkQueueSubmit2KHR"));
    }

  if(props.hasDynRendering) {
    vkCmdBeginRenderingKHR = PFN_vkCmdBeginRenderingKHR(vkGetDeviceProcAddr(device.impl,"vkCmdBeginRenderingKHR"));
    vkCmdEndRenderingKHR   = PFN_vkCmdEndRenderingKHR  (vkGetDeviceProcAddr(device.impl,"vkCmdEndRenderingKHR"));
    }

  if(props.hasDeviceAddress) {
    vkGetBufferDeviceAddress = PFN_vkGetBufferDeviceAddressKHR(vkGetDeviceProcAddr(device.impl,"vkGetBufferDeviceAddressKHR"));
    }

  if(props.raytracing.rayQuery) {
    vkCreateAccelerationStructure        = PFN_vkCreateAccelerationStructureKHR(vkGetDeviceProcAddr(device.impl,"vkCreateAccelerationStructureKHR"));
    vkDestroyAccelerationStructure       = PFN_vkDestroyAccelerationStructureKHR(vkGetDeviceProcAddr(device.impl,"vkDestroyAccelerationStructureKHR"));
    vkGetAccelerationStructureBuildSizes = PFN_vkGetAccelerationStructureBuildSizesKHR(vkGetDeviceProcAddr(device.impl,"vkGetAccelerationStructureBuildSizesKHR"));
    vkCmdBuildAccelerationStructures     = PFN_vkCmdBuildAccelerationStructuresKHR(vkGetDeviceProcAddr(device.impl,"vkCmdBuildAccelerationStructuresKHR"));
    }

  if(props.raytracing.rayQuery && props.hasDeviceAddress) {
    vkGetAccelerationStructureDeviceAddress = PFN_vkGetAccelerationStructureDeviceAddressKHR(vkGetDeviceProcAddr(device.impl,"vkGetAccelerationStructureDeviceAddressKHR"));
    }

  if(props.meshlets.meshShader) {
    vkCmdDrawMeshTasks         = PFN_vkCmdDrawMeshTasksEXT(vkGetDeviceProcAddr(device.impl,"vkCmdDrawMeshTasksEXT"));
    vkCmdDrawMeshTasksIndirect = PFN_vkCmdDrawMeshTasksIndirectEXT(vkGetDeviceProcAddr(device.impl,"vkCmdDrawMeshTasksIndirectEXT"));
    }

  if(props.hasDebugMarker) {
    vkCmdDebugMarkerBegin      = PFN_vkCmdDebugMarkerBeginEXT     (vkGetDeviceProcAddr(device.impl,"vkCmdDebugMarkerBeginEXT"));
    vkCmdDebugMarkerEnd        = PFN_vkCmdDebugMarkerEndEXT       (vkGetDeviceProcAddr(device.impl,"vkCmdDebugMarkerEndEXT"));
    vkDebugMarkerSetObjectName = PFN_vkDebugMarkerSetObjectNameEXT(vkGetDeviceProcAddr(device.impl,"vkDebugMarkerSetObjectNameEXT"));
    }
  dummyIfNull(vkCmdDebugMarkerBegin);
  dummyIfNull(vkCmdDebugMarkerEnd);
  dummyIfNull(vkDebugMarkerSetObjectName);
  }

VDevice::MemIndex VDevice::memoryTypeIndex(uint32_t typeBits, VkMemoryPropertyFlags props, VkImageTiling tiling) const {
  for(size_t i=0; i<memoryProperties.memoryTypeCount; ++i) {
    auto bit = (uint32_t(1) << i);
    if((typeBits & bit)==0)
      continue;
    if(memoryProperties.memoryTypes[i].propertyFlags==props) {
      MemIndex ret;
      ret.typeId      = uint32_t(i);
      // avoid bufferImageGranularity shenanigans
      ret.heapId      = (tiling==VK_IMAGE_TILING_OPTIMAL && this->props.bufferImageGranularity>1) ? ret.typeId*2+1 : ret.typeId*2+0;
      ret.hostVisible = (memoryProperties.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
      return ret;
      }
    }
  for(size_t i=0; i<memoryProperties.memoryTypeCount; ++i) {
    auto bit = (uint32_t(1) << i);
    if((typeBits & bit)==0)
      continue;
    if((memoryProperties.memoryTypes[i].propertyFlags & props)==props) {
      MemIndex ret;
      ret.typeId = uint32_t(i);
      // avoid bufferImageGranularity shenanigans
      ret.heapId      = (tiling==VK_IMAGE_TILING_OPTIMAL && this->props.bufferImageGranularity>1) ? ret.typeId*2+1 : ret.typeId*2+0;
      ret.hostVisible = (memoryProperties.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
      return ret;
      }
    }
  MemIndex ret;
  ret.typeId = uint32_t(-1);
  return ret;
  }

VBuffer& VDevice::dummySsbo() {
  // NOTE: null desriptor is part of VK_EXT_robustness2
  std::lock_guard<std::mutex> guard(syncSsbo);
  if(dummySsboVal.impl==VK_NULL_HANDLE) {
    // in principle 1 byte is also fine
    dummySsboVal = allocator.alloc(nullptr, sizeof(uint32_t), MemUsage::StorageBuffer, BufferHeap::Device);
    }
  return dummySsboVal;
  }

uint32_t VDevice::roundUpDescriptorCount(ShaderReflection::Class cls, size_t cnt) {
  uint32_t cntRound = 0;
  if(cnt<64)
    cntRound = 64;
  else if(cnt<128)
    cntRound = 128;
  else if(cnt<256)
    cntRound = 256;
  else if(cnt<512)
    cntRound = 512;
  else if(cnt<1024)
    cntRound = 1024;
  else
    cntRound = uint32_t((cnt+2048-1)/2048)*2048;

  switch(cls) {
    case ShaderReflection::Texture:
    case ShaderReflection::Image:
    case ShaderReflection::ImgR:
    case ShaderReflection::ImgRW:
      cntRound = std::min(cntRound, props.descriptors.maxTexture);
      break;
    case ShaderReflection::Sampler:
      cntRound = std::min(cntRound, props.descriptors.maxSamplers);
      break;
    case ShaderReflection::SsboR:
    case ShaderReflection::SsboRW:
    case ShaderReflection::Tlas:
      cntRound = std::min(cntRound, props.descriptors.maxStorage);
      break;
    case ShaderReflection::Ubo:
    case ShaderReflection::Push:
    case ShaderReflection::Count:
      break;
    }

  return cntRound;
  }

VkDescriptorSetLayout VDevice::bindlessArrayLayout(ShaderReflection::Class cls, size_t cnt) {
  // WA for Intel linux-drivers. They have bugs with VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT
  // Details: https://github.com/Try/OpenGothic/issues/741

  ShaderReflection::LayoutDesc lx;
  lx.bindings[0] = cls;
  lx.stage[0]    = ShaderReflection::None;
  lx.count[0]    = uint32_t(cnt);
  lx.runtime     = 0x1;
  lx.array       = 0x1;
  lx.active      = 0x1;

  return setLayouts.findLayout(lx);
  }

void VDevice::waitIdle() {
  waitIdleSync(queues,sizeof(queues)/sizeof(queues[0]));
  }

void VDevice::waitIdleSync(VDevice::Queue* q, size_t n) {
  if(n==0) {
    vkDeviceWaitIdle(device.impl);
    return;
    }
  if(q->impl!=nullptr) {
    std::lock_guard<std::mutex> guard(q->sync);
    waitIdleSync(q+1,n-1);
    } else {
    waitIdleSync(q+1,n-1);
    }
  }

void VDevice::submit(VCommandBuffer& cmd, VFence* sync) {
  size_t waitCnt = 0;
  for(auto& s:cmd.swapchainSync) {
    if(s->state!=Detail::VSwapchain::S_Pending)
      continue;
    s->state = Detail::VSwapchain::S_Aquired;
    ++waitCnt;
    }

  SmallArray<VkSemaphore, 32> wait(waitCnt);
  size_t                      waitId  = 0;
  for(auto& s:cmd.swapchainSync) {
    if(s->state!=Detail::VSwapchain::S_Aquired)
      continue;
    s->state = Detail::VSwapchain::S_Draw;
    wait[waitId] = s->acquire;
    ++waitId;
    }

  VkFence fence = VK_NULL_HANDLE;
  if(sync!=nullptr) {
    sync->reset();
    fence = sync->impl;
    }

  if(vkQueueSubmit2!=nullptr) {
    SmallArray<VkSemaphoreSubmitInfoKHR, 32> wait2(waitCnt);
    for(size_t i=0; i<waitCnt; ++i) {
      wait2[i].sType       = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO_KHR;
      wait2[i].pNext       = nullptr;
      wait2[i].semaphore   = wait[i];
      wait2[i].value       = 0;
      wait2[i].stageMask   = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
      wait2[i].deviceIndex = 0;
      }
    SmallArray<VkCommandBufferSubmitInfoKHR,MaxCmdChunks> flat(cmd.chunks.size());
    auto node = cmd.chunks.begin();
    for(size_t i=0; i<cmd.chunks.size(); ++i) {
      flat[i].sType         = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO_KHR;
      flat[i].pNext         = nullptr;
      flat[i].commandBuffer = node->val[i%cmd.chunks.chunkSize].impl;
      flat[i].deviceMask    = 0;
      if(i+1==cmd.chunks.chunkSize)
        node = node->next;
      }

    VkSubmitInfo2KHR submitInfo = {};
    submitInfo.sType                  = VK_STRUCTURE_TYPE_SUBMIT_INFO_2_KHR;
    submitInfo.commandBufferInfoCount = uint32_t(cmd.chunks.size());
    submitInfo.pCommandBufferInfos    = flat.get();
    submitInfo.waitSemaphoreInfoCount = uint32_t(waitCnt);
    submitInfo.pWaitSemaphoreInfos    = wait2.get();

    graphicsQueue->submit(1,&submitInfo,fence,vkQueueSubmit2);
    } else {
    SmallArray<VkPipelineStageFlags, 32> waitStages(waitCnt);
    for(size_t i=0; i<waitCnt; ++i) {
      // NOTE: our sw images are draw-only
      waitStages[i] = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
      }

    SmallArray<VkCommandBuffer,MaxCmdChunks> flat(cmd.chunks.size());
    auto node = cmd.chunks.begin();
    for(size_t i=0; i<cmd.chunks.size(); ++i) {
      flat[i] = node->val[i%cmd.chunks.chunkSize].impl;
      if(i+1==cmd.chunks.chunkSize)
        node = node->next;
      }
    VkSubmitInfo submitInfo = {};
    submitInfo.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = uint32_t(cmd.chunks.size());
    submitInfo.pCommandBuffers    = flat.get();
    submitInfo.waitSemaphoreCount = uint32_t(waitCnt);
    submitInfo.pWaitSemaphores    = wait.get();
    submitInfo.pWaitDstStageMask  = waitStages.get();

    graphicsQueue->submit(1,&submitInfo,fence);
    }
  }

void VDevice::Queue::waitIdle() {
  std::lock_guard<std::mutex> guard(sync);
  vkAssert(vkQueueWaitIdle(impl));
  }

void VDevice::Queue::submit(uint32_t submitCount, const VkSubmitInfo* pSubmits, VkFence fence) {
  std::lock_guard<std::mutex> guard(sync);
  vkAssert(vkQueueSubmit(impl,submitCount,pSubmits,fence));
  }

void VDevice::Queue::submit(uint32_t submitCount, const VkSubmitInfo2KHR* pSubmits, VkFence fence, PFN_vkQueueSubmit2KHR vkQueueSubmit2) {
  std::lock_guard<std::mutex> guard(sync);
  vkAssert(vkQueueSubmit2(impl,submitCount,pSubmits,fence));
  }

VkResult VDevice::Queue::present(VkPresentInfoKHR& presentInfo) {
  std::lock_guard<std::mutex> guard(sync);
  return vkQueuePresentKHR(impl,&presentInfo);
  }

#endif
