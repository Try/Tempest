#if defined(TEMPEST_BUILD_VULKAN)

#include "vdevice.h"

#include "vcommandbuffer.h"
#include "vcommandpool.h"
#include "vfence.h"
#include "vswapchain.h"
#include "vbuffer.h"
#include "vtexture.h"
#include "vmeshlethelper.h"
#include "system/api/x11api.h"

#include <Tempest/Log>
#include <Tempest/Platform>
#include <thread>
#include <set>
#include <cstring>
#include <array>

#if defined(__WINDOWS__)
#  define VK_USE_PLATFORM_WIN32_KHR
#  include <windows.h>
#  include <vulkan/vulkan_win32.h>
#elif defined(__LINUX__)
#  define VK_USE_PLATFORM_XLIB_KHR
#  include <X11/Xlib.h>
#  include <vulkan/vulkan_xlib.h>
#  undef Always
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

VDevice::VDevice(VulkanInstance &api, const char* gpuName)
  :instance(api.instance), fboMap(*this) {
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
#elif defined(__LINUX__)
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

bool VDevice::isDeviceSuitable(VkPhysicalDevice device, const char* gpuName) {
  if(gpuName!=nullptr) {
    VkPhysicalDeviceProperties prop={};
    vkGetPhysicalDeviceProperties(device,&prop);
    if(std::strcmp(gpuName,prop.deviceName)!=0)
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
#elif defined(__LINUX__)
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

VDevice::SwapChainSupport VDevice::querySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface) {
  SwapChainSupport details;

  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

  uint32_t formatCount = 0;
  vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);

  if(formatCount != 0){
    details.formats.resize(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
    }

  uint32_t presentModeCount = 0;
  vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);

  if(presentModeCount != 0){
    details.presentModes.resize(presentModeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
    }

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
  if(props.raytracing.rayQuery) {
    rqExt.push_back(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
    rqExt.push_back(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
    rqExt.push_back(VK_KHR_SPIRV_1_4_EXTENSION_NAME);
    rqExt.push_back(VK_KHR_RAY_QUERY_EXTENSION_NAME);
    }
  if(props.meshlets.meshShader) {
    rqExt.push_back(VK_NV_MESH_SHADER_EXTENSION_NAME);
    }
  if(props.hasDescIndexing) {
    rqExt.push_back(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME);
    }
  if(props.hasDynRendering) {
    rqExt.push_back(VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME);
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

    VkPhysicalDeviceMeshShaderFeaturesNV meshFeatures = {};
    meshFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_NV;

    VkPhysicalDeviceDescriptorIndexingFeatures indexingFeatures = {};
    indexingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;

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

    auto vkGetPhysicalDeviceFeatures2 = PFN_vkGetPhysicalDeviceFeatures2  (vkGetInstanceProcAddr(instance,"vkGetPhysicalDeviceFeatures2KHR"));

    vkGetPhysicalDeviceFeatures2(pdev, &features);
    bdaFeatures.bufferDeviceAddressCaptureReplay  = VK_FALSE;
    bdaFeatures.bufferDeviceAddressMultiDevice    = VK_FALSE;
    asFeatures.accelerationStructureCaptureReplay = VK_FALSE;
    asFeatures.accelerationStructureIndirectBuild = VK_FALSE;
    asFeatures.accelerationStructureHostCommands  = VK_FALSE;
    asFeatures.descriptorBindingAccelerationStructureUpdateAfterBind = VK_FALSE;

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
    vkCmdDrawMeshTasks = PFN_vkCmdDrawMeshTasksNV(vkGetDeviceProcAddr(device.impl,"vkCmdDrawMeshTasksNV"));
    }
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

void VDevice::allocMeshletHelper() {
  std::unique_lock<std::mutex> guard(meshSync);
  if(meshHelper==nullptr)
    meshHelper.reset(new VMeshletHelper(*this));
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
    s->state = Detail::VSwapchain::S_Draw0;
    ++waitCnt;
    }

  SmallArray<VkSemaphore, 32>          wait      (waitCnt);
  SmallArray<VkPipelineStageFlags, 32> waitStages(waitCnt);
  size_t                               waitId  = 0;
  for(auto& s:cmd.swapchainSync) {
    if(s->state!=Detail::VSwapchain::S_Draw0)
      continue;
    s->state = Detail::VSwapchain::S_Draw1;
    wait[waitId] = s->aquire;
    ++waitId;
    }

  for(size_t i=0; i<waitCnt; ++i) {
    // NOTE: our sw images are draw-only
    waitStages[i] = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    }

  VkFence fence = VK_NULL_HANDLE;
  if(sync!=nullptr) {
    sync->reset();
    fence = sync->impl;
    }

  if(vkQueueSubmit2!=nullptr) {
    SmallArray<VkCommandBufferSubmitInfoKHR,64> flat(cmd.chunks.size());
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

    graphicsQueue->submit(1,&submitInfo,fence,vkQueueSubmit2);
    } else {
    SmallArray<VkCommandBuffer,64> flat(cmd.chunks.size());
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

    graphicsQueue->submit(1,&submitInfo,fence);
    }
  }

void Tempest::Detail::VDevice::Queue::waitIdle() {
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
