#if defined(TEMPEST_BUILD_VULKAN)

#include "vdevice.h"

#include "vcommandbuffer.h"
#include "vfence.h"
#include "vswapchain.h"

#include <Tempest/Application>
#include <Tempest/Log>
#include <cstring>
#include <array>

using namespace Tempest;
using namespace Tempest::Detail;

template<class T, class ... Args>
T min(T t, Args... args) {
  T ax[] = {t, args...};
  return *std::min_element(std::begin(ax), std::end(ax));
  }

template<class T, class ... Args>
T max(T t, Args... args) {
  T ax[] = {t, args...};
  return *std::max_element(std::begin(ax), std::end(ax));
  }

template<class R, class ... Args>
void dummyIfNull(R (*&fn)(Args...a)) {
  struct Dummy {
    static R fn(Args...a) { return R(); }
    };

  if(fn==nullptr)
    fn = Dummy::fn;
  }

static bool extensionSupport(const std::vector<VkExtensionProperties>& list, const char* name) {
  for(auto& r:list)
    if(std::strcmp(name,r.extensionName)==0)
      return true;
  return false;
  }


const std::initializer_list<const char*> VDevice::requiredExtensions = {
  VK_KHR_SWAPCHAIN_EXTENSION_NAME
  };

VDevice::autoDevice::~autoDevice() {
  vkDestroyDevice(impl,nullptr);
  }

bool VDevice::VkProps::hasFilteredFormat(TextureFormat f) const {
  uint64_t  m = uint64_t(1) << uint64_t(f);
  return (filteredLinearFormat&m)!=0;
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


VDevice::VDevice(VkInstance instance, const bool hasDeviceFeatures2, VkPhysicalDevice pdev)
  :instance(instance), hasDeviceFeatures2(hasDeviceFeatures2),
    fboMap(*this), setLayouts(*this), psoLayouts(*this), descPool(*this), bindless(*this) {
  deviceProps(instance, hasDeviceFeatures2, pdev, props);
  deviceQueueProps(pdev, props);

  createLogicalDevice(pdev);
  vkGetPhysicalDeviceMemoryProperties(pdev, &memoryProperties);

  physicalDevice = pdev;
  allocator.setDevice(*this);
  descPool.setupLimits();
  data.reset(new DataMgr(*this));
  }

VDevice::~VDevice() {
  vkDeviceWaitIdle(device.impl);
  data.reset();

  for(auto& i:timeline.timepoint) {
    if(i==nullptr)
      continue;
    vkDestroyFence(device.impl, i->fence, nullptr);
    }
  }

void VDevice::createLogicalDevice(VkPhysicalDevice pdev) {
  std::array<uint32_t,2>  uniqueQueueFamilies = {props.graphicsFamily, props.presentFamily};
  float                   queuePriority       = 1.0f;
  size_t                  queueCnt            = 0;
  VkDeviceQueueCreateInfo qinfo[3]            = {};
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

  if(hasDeviceFeatures2) {
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

void VDevice::deviceProps(VkInstance instance, const bool hasDeviceFeatures2, VkPhysicalDevice physicalDevice, VkProps& props) {
  const auto ext = extensionsList(physicalDevice);
  if(extensionSupport(ext,VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME))
    props.hasMemRq2 = true;
  if(extensionSupport(ext,VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME))
    props.hasDedicatedAlloc = true;
  if(hasDeviceFeatures2 && extensionSupport(ext,VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME))
    props.hasSync2 = true;
  if(hasDeviceFeatures2 && extensionSupport(ext,VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME))
    props.hasDeviceAddress = true;
  if(hasDeviceFeatures2 && extensionSupport(ext,VK_KHR_SPIRV_1_4_EXTENSION_NAME)) {
    props.hasSpirv_1_4 = true;
    }
  if(hasDeviceFeatures2 &&
     extensionSupport(ext,VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME) &&
     extensionSupport(ext,VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME) &&
     extensionSupport(ext,VK_KHR_RAY_QUERY_EXTENSION_NAME)) {
    props.raytracing.rayQuery = true;
    }
  if(hasDeviceFeatures2 && extensionSupport(ext,VK_EXT_MESH_SHADER_EXTENSION_NAME)) {
    props.meshlets.meshShader = true;
    }
  if(hasDeviceFeatures2 && extensionSupport(ext,VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME)) {
    props.hasDescIndexing = true;
    }
  if(hasDeviceFeatures2 && extensionSupport(ext,VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME)) {
    props.hasDynRendering = true;
    }
  if(hasDeviceFeatures2 && extensionSupport(ext,VK_KHR_FRAGMENT_SHADER_BARYCENTRIC_EXTENSION_NAME)) {
    props.hasBarycentrics = true;
    }
  if(hasDeviceFeatures2 && extensionSupport(ext,VK_EXT_ROBUSTNESS_2_EXTENSION_NAME)) {
    props.hasRobustness2 = true;
    }
  if(extensionSupport(ext,VK_EXT_DEBUG_MARKER_EXTENSION_NAME)) {
    props.hasDebugMarker = true;
    }
  if(extensionSupport(ext,VK_EXT_LOAD_STORE_OP_NONE_EXTENSION_NAME)) {
    props.hasStoreOpNone = true;
    }
  if(extensionSupport(ext,VK_KHR_MAINTENANCE_1_EXTENSION_NAME)) {
    props.hasMaintenance1 = true;
    }
  if(extensionSupport(ext,VK_KHR_VULKAN_MEMORY_MODEL_EXTENSION_NAME)) {
    props.memoryModel = true;
    }

  VkPhysicalDeviceProperties prop = {};
  vkGetPhysicalDeviceProperties(physicalDevice, &prop);
  props.nonCoherentAtomSize = size_t(prop.limits.nonCoherentAtomSize);
  if(props.nonCoherentAtomSize==0)
    props.nonCoherentAtomSize=1;

  props.bufferImageGranularity = size_t(prop.limits.bufferImageGranularity);
  if(props.bufferImageGranularity==0)
    props.bufferImageGranularity=1;

  props.vendorID = prop.vendorID;

  VkPhysicalDeviceProperties devP = {};
  vkGetPhysicalDeviceProperties(physicalDevice, &devP);

  VkPhysicalDeviceFeatures supportedFeatures = {};
  vkGetPhysicalDeviceFeatures(physicalDevice, &supportedFeatures);

  VkPhysicalDeviceFeatures deviceFeatures = {};
  deviceFeatures.samplerAnisotropy    = supportedFeatures.samplerAnisotropy;
  deviceFeatures.textureCompressionBC = supportedFeatures.textureCompressionBC;
  deviceFeatures.tessellationShader   = supportedFeatures.tessellationShader;
  deviceFeatures.geometryShader       = supportedFeatures.geometryShader;
  deviceFeatures.fillModeNonSolid     = supportedFeatures.fillModeNonSolid;

  deviceFeatures.vertexPipelineStoresAndAtomics = supportedFeatures.vertexPipelineStoresAndAtomics;
  deviceFeatures.fragmentStoresAndAtomics       = supportedFeatures.fragmentStoresAndAtomics;

  // non-bindless limit
  props.descriptors.maxSamplers = std::max(devP.limits.maxDescriptorSetSamplers,
                                           devP.limits.maxPerStageDescriptorSamplers);
  props.descriptors.maxTexture = std::max(devP.limits.maxDescriptorSetSampledImages,
                                          devP.limits.maxPerStageDescriptorSampledImages);
  props.descriptors.maxStorage = max(devP.limits.maxDescriptorSetStorageBuffers,
                                     devP.limits.maxPerStageDescriptorStorageBuffers,
                                     devP.limits.maxDescriptorSetStorageImages,
                                     devP.limits.maxPerStageDescriptorStorageImages);

  if(hasDeviceFeatures2) {
    VkPhysicalDeviceFeatures2 features = {};
    features.sType    = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2_KHR;
    features.features = deviceFeatures;

    VkPhysicalDeviceProperties2 properties = {};
    properties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2_KHR;

    VkPhysicalDeviceDynamicRenderingFeaturesKHR dynRendering = {};
    dynRendering.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR;

    VkPhysicalDeviceSynchronization2FeaturesKHR sync2 = {};
    sync2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SYNCHRONIZATION_2_FEATURES_KHR;

    VkPhysicalDeviceBufferDeviceAddressFeaturesKHR bdaFeatures = {};
    bdaFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES_KHR;

    VkPhysicalDeviceAccelerationStructureFeaturesKHR asFeatures = {};
    asFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;

    VkPhysicalDeviceAccelerationStructurePropertiesKHR asProperties = {};
    asProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_PROPERTIES_KHR;

    VkPhysicalDeviceRayQueryFeaturesKHR rayQueryFeatures = {};
    rayQueryFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR;

    VkPhysicalDeviceMeshShaderFeaturesEXT meshFeatures = {};
    meshFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT;

    VkPhysicalDeviceMeshShaderPropertiesEXT meshProperties = {};
    meshProperties.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_PROPERTIES_EXT;

    VkPhysicalDeviceDescriptorIndexingFeatures indexingFeatures = {};
    indexingFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_FEATURES;

    VkPhysicalDeviceDescriptorIndexingProperties indexingProps = {};
    indexingProps.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DESCRIPTOR_INDEXING_PROPERTIES;

    VkPhysicalDeviceRobustness2PropertiesEXT rebustness2Props = {};
    rebustness2Props.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ROBUSTNESS_2_PROPERTIES_EXT;

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

      asProperties.pNext = properties.pNext;
      properties.pNext = &asProperties;
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

      meshProperties.pNext = properties.pNext;
      properties.pNext = &meshProperties;
      }
    if(props.hasDescIndexing) {
      indexingFeatures.pNext = features.pNext;
      features.pNext = &indexingFeatures;

      indexingProps.pNext = properties.pNext;
      properties.pNext = &indexingProps;
      }
    if(props.hasRobustness2) {
      rebustness2Props.pNext = properties.pNext;
      properties.pNext = &rebustness2Props;
      }
    if(props.memoryModel) {
      memoryFeatures.pNext = features.pNext;
      features.pNext = &memoryFeatures;
      }

    auto vkGetPhysicalDeviceFeatures2   = PFN_vkGetPhysicalDeviceFeatures2  (vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceFeatures2KHR"));
    auto vkGetPhysicalDeviceProperties2 = PFN_vkGetPhysicalDeviceProperties2(vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceProperties2KHR"));

    vkGetPhysicalDeviceFeatures2  (physicalDevice, &features);
    vkGetPhysicalDeviceProperties2(physicalDevice, &properties);

    props.hasSync2                = (sync2.synchronization2==VK_TRUE);
    props.hasDynRendering         = (dynRendering.dynamicRendering==VK_TRUE);
    props.hasDeviceAddress        = (bdaFeatures.bufferDeviceAddress==VK_TRUE);
    props.raytracing.rayQuery     = (rayQueryFeatures.rayQuery==VK_TRUE);
    props.meshlets.taskShader     = (meshFeatures.taskShader==VK_TRUE);
    props.meshlets.meshShader     = (meshFeatures.meshShader==VK_TRUE);
    props.meshlets.maxGroups.x    = meshProperties.maxMeshWorkGroupCount[0];
    props.meshlets.maxGroups.y    = meshProperties.maxMeshWorkGroupCount[1];
    props.meshlets.maxGroups.z    = meshProperties.maxMeshWorkGroupCount[2];
    props.meshlets.maxGroupSize.x = meshProperties.maxMeshWorkGroupSize[0];
    props.meshlets.maxGroupSize.y = meshProperties.maxMeshWorkGroupSize[1];
    props.meshlets.maxGroupSize.z = meshProperties.maxMeshWorkGroupSize[2];

    props.accelerationStructureScratchOffsetAlignment = asProperties.minAccelerationStructureScratchOffsetAlignment;

    //props.meshlets.meshShader = false;

    if(indexingFeatures.runtimeDescriptorArray!=VK_FALSE) {
      // NOTE1: no UBO support - won't do
      // NOTE2: no Storage-image support on Intel
      props.descriptors.nonUniformIndexing =
          (indexingFeatures.shaderSampledImageArrayNonUniformIndexing ==VK_TRUE) &&
          (indexingFeatures.shaderStorageBufferArrayNonUniformIndexing==VK_TRUE);
      props.descriptors.nonUniformIndexing &=
        (indexingFeatures.descriptorBindingSampledImageUpdateAfterBind ==VK_TRUE) &&
        (indexingFeatures.descriptorBindingStorageBufferUpdateAfterBind==VK_TRUE);
      }

    if(indexingFeatures.runtimeDescriptorArray!=VK_FALSE) {
      props.descriptors.maxSamplers = std::max(indexingProps.maxDescriptorSetUpdateAfterBindSamplers,
                                               indexingProps.maxPerStageDescriptorUpdateAfterBindSamplers);

      props.descriptors.maxTexture  = std::max(indexingProps.maxDescriptorSetUpdateAfterBindSampledImages,
                                               indexingProps.maxPerStageDescriptorUpdateAfterBindSampledImages);

      props.descriptors.maxStorage  = max(indexingProps.maxDescriptorSetUpdateAfterBindStorageBuffers,
                                          indexingProps.maxPerStageDescriptorUpdateAfterBindStorageBuffers,
                                          indexingProps.maxDescriptorSetUpdateAfterBindStorageImages,
                                          indexingProps.maxPerStageDescriptorUpdateAfterBindStorageImages);
      }

    if(asFeatures.descriptorBindingAccelerationStructureUpdateAfterBind!=VK_FALSE) {
      props.descriptors.maxStorage = max(props.descriptors.maxStorage,
                                         asProperties.maxDescriptorSetAccelerationStructures,
                                         asProperties.maxPerStageDescriptorAccelerationStructures);
      }

    if(memoryFeatures.vulkanMemoryModel!=VK_FALSE) {
      props.memoryModel = true;
      }
    }

  std::memcpy(props.name,devP.deviceName,sizeof(props.name));

  props.vbo.maxAttribs    = size_t(devP.limits.maxVertexInputAttributes);
  props.vbo.maxStride     = size_t(devP.limits.maxVertexInputBindingStride);

  props.ibo.maxValue      = size_t(devP.limits.maxDrawIndexedIndexValue);

  props.ssbo.offsetAlign  = size_t(devP.limits.minStorageBufferOffsetAlignment);
  props.ssbo.maxRange     = size_t(devP.limits.maxStorageBufferRange);

  props.ubo.offsetAlign   = size_t(devP.limits.minUniformBufferOffsetAlignment);
  props.ubo.maxRange      = size_t(devP.limits.maxUniformBufferRange);

  props.push.maxRange     = size_t(devP.limits.maxPushConstantsSize);

  props.anisotropy        = supportedFeatures.samplerAnisotropy;
  props.maxAnisotropy     = devP.limits.maxSamplerAnisotropy;
  props.tesselationShader = supportedFeatures.tessellationShader;
  props.geometryShader    = supportedFeatures.geometryShader;
  if(props.vendorID==0x5143) {
    // QCom
    props.tesselationShader = false;
    props.geometryShader    = false;
    }

  props.storeAndAtomicVs  = supportedFeatures.vertexPipelineStoresAndAtomics;
  props.storeAndAtomicFs  = supportedFeatures.fragmentStoresAndAtomics;

  props.render.maxColorAttachments  = devP.limits.maxColorAttachments;
  props.render.maxViewportSize.w    = devP.limits.maxViewportDimensions[0];
  props.render.maxViewportSize.h    = devP.limits.maxViewportDimensions[1];
  props.render.maxClipCullDistances = min(devP.limits.maxCombinedClipAndCullDistances, devP.limits.maxClipDistances, devP.limits.maxCullDistances);

  props.compute.maxGroups.x = devP.limits.maxComputeWorkGroupCount[0];
  props.compute.maxGroups.y = devP.limits.maxComputeWorkGroupCount[1];
  props.compute.maxGroups.z = devP.limits.maxComputeWorkGroupCount[2];

  props.compute.maxGroupSize.x  = devP.limits.maxComputeWorkGroupSize[0];
  props.compute.maxGroupSize.y  = devP.limits.maxComputeWorkGroupSize[1];
  props.compute.maxGroupSize.z  = devP.limits.maxComputeWorkGroupSize[2];
  props.compute.maxInvocations  = devP.limits.maxComputeWorkGroupInvocations;
  props.compute.maxSharedMemory = devP.limits.maxComputeSharedMemorySize;

  props.tex2d.maxSize = devP.limits.maxImageDimension2D;
  props.tex3d.maxSize = devP.limits.maxImageDimension3D;

  switch(devP.deviceType) {
    case VK_PHYSICAL_DEVICE_TYPE_CPU:
      props.type = DeviceType::Cpu;
      break;
    case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
      props.type = DeviceType::Virtual;
      break;
    case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
      props.type = DeviceType::Integrated;
      break;
    case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
      props.type = DeviceType::Discrete;
      break;
    default:
      props.type = DeviceType::Unknown;
      break;
    }

  deviceFormatProps(physicalDevice, props);
  }

void VDevice::deviceFormatProps(VkPhysicalDevice device, VkProps& props) {
  /*
   * formats support table: https://registry.khronos.org/vulkan/specs/latest/html/vkspec.html#features-required-format-support
   *   sampled image must also have transfer bits by spec.
   *   mipmap generation, in engine, implemented by blit-shaders.
   */
  VkFormatFeatureFlags imageRqFlags    = VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT|VK_FORMAT_FEATURE_TRANSFER_SRC_BIT|VK_FORMAT_FEATURE_TRANSFER_DST_BIT;
  VkFormatFeatureFlags mipmapsFlags    = VK_FORMAT_FEATURE_BLIT_DST_BIT|VK_FORMAT_FEATURE_BLIT_SRC_BIT;
  VkFormatFeatureFlags attachRqFlags   = VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT; //|VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT;
  VkFormatFeatureFlags depthAttFlags   = VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;
  VkFormatFeatureFlags storageAttFlags = VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT;
  VkFormatFeatureFlags atomicFlags     = VK_FORMAT_FEATURE_STORAGE_IMAGE_ATOMIC_BIT;

  uint64_t smpFormat=0, attFormat=0, dattFormat=0, storFormat=0, atomFormat=0, filteredLinearFormat=0;
  for(uint32_t i=0;i<TextureFormat::Last;++i){
    VkFormat f = Detail::nativeFormat(TextureFormat(i));

    VkFormatProperties frm={};
    vkGetPhysicalDeviceFormatProperties(device,f,&frm);
    if(isDepthFormat(TextureFormat(i))) {
      if((frm.optimalTilingFeatures & imageRqFlags)==imageRqFlags){
        smpFormat |= (1ull<<i);
        }
      }
    else if(isCompressedFormat(TextureFormat(i))) {
      if((frm.optimalTilingFeatures & imageRqFlags)==imageRqFlags){
        smpFormat |= (1ull<<i);
        }
      }
    else {
      if((frm.optimalTilingFeatures & imageRqFlags)==imageRqFlags &&
         (frm.optimalTilingFeatures & mipmapsFlags)==mipmapsFlags){
        smpFormat |= (1ull<<i);
        }
      }

    if((frm.optimalTilingFeatures & attachRqFlags)==attachRqFlags){
      attFormat |= (1ull<<i);
      }
    if((frm.optimalTilingFeatures & depthAttFlags)==depthAttFlags){
      dattFormat |= (1ull<<i);
      }
    if((frm.optimalTilingFeatures & storageAttFlags)==storageAttFlags){
      storFormat |= (1ull<<i);
      }
    if((frm.optimalTilingFeatures & atomicFlags)==atomicFlags){
      atomFormat |= (1ull<<i);
      }
    if((frm.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)==VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT){
      filteredLinearFormat |= (1ull<<i);
      }
    }

  props.setSamplerFormats(smpFormat);
  props.setAttachFormats (attFormat);
  props.setDepthFormats  (dattFormat);
  props.setStorageFormats(storFormat);
  props.setAtomicFormats (atomFormat & storFormat);

  props.filteredLinearFormat = filteredLinearFormat;
  }

std::vector<VkExtensionProperties> VDevice::extensionsList(VkPhysicalDevice dev) {
  uint32_t extensionCount;
  vkEnumerateDeviceExtensionProperties(dev, nullptr, &extensionCount, nullptr);

  std::vector<VkExtensionProperties> ext(extensionCount);
  vkEnumerateDeviceExtensionProperties(dev, nullptr, &extensionCount, ext.data());
  return ext;
  }

void VDevice::deviceQueueProps(VkPhysicalDevice device, VkProps& props) {
  uint32_t queueFamilyCount = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

  std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
  vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

  uint32_t graphics  = uint32_t(-1);
  uint32_t present   = uint32_t(-1);
  uint32_t universal = uint32_t(-1);

  for(uint32_t i=0; i<queueFamilyCount; ++i) {
    const auto& queueFamily = queueFamilies[i];
    if(queueFamily.queueCount<=0)
      continue;

    static const VkQueueFlags rqFlag = (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT);

    const bool graphicsSupport = ((queueFamily.queueFlags & rqFlag)==rqFlag);
    const bool presentSupport  = VSwapchain::checkPresentSupport(device, i);

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

  props.graphicsFamily = graphics;
  props.presentFamily  = present;
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

std::unique_lock<std::mutex> VDevice::compilerLock() {
  if(props.vendorID==0x5143) {
    //WA for QCom drivers
    static std::atomic_bool once = {};
    if(once.exchange(true)==false)
      Log::d("VulkanApi: Enable workaround for driver crash on QCom");
    static std::mutex sync;
    return std::unique_lock<std::mutex>(sync);
    }
  return std::unique_lock<std::mutex>();
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

std::shared_ptr<VFence> VDevice::findAvailableFence() {
  for(int pass=0; pass<2; ++pass) {
    for(uint32_t id=0; id<MaxFences; ++id) {
      auto& i = timeline.timepoint[id];
      if(i==nullptr)
        continue;
      if(i->status!=VK_SUCCESS)
        continue;
      if(i.use_count()>1 && pass==0)
        continue;
      if(i.use_count()>1) {
        auto fence = i->fence;
        //NOTE: application may still hold references to `i`
        i->fence = VK_NULL_HANDLE;
        i = std::make_shared<VFence>(this, fence, id);
        }
      vkAssert(vkResetFences(device.impl, 1, &i->fence));
      return i;
      }
    }
  return nullptr;
  }

void VDevice::waitAny(uint64_t timeout) {
  VkFence  fences[MaxFences] = {};
  uint32_t num = 0;

  for(auto& i:timeline.timepoint) {
    if(i==nullptr || i->status==VK_SUCCESS)
      continue;
    fences[num] = i->fence;
    ++num;
    }

  const auto ret = vkWaitForFences(device.impl, num, fences, VK_FALSE, timeout);
  if(ret==VK_TIMEOUT)
    return;
  vkAssert(ret);

  // refresh status
  for(uint32_t id=0; id<MaxFences; ++id) {
    auto& i = timeline.timepoint[id];
    if(i==nullptr)
      continue;
    i->status = vkGetFenceStatus(device.impl, i->fence);
    }
  }

std::shared_ptr<VFence> VDevice::aquireFence() {
  std::lock_guard<std::mutex> guard(timeline.sync);

  // reuse signalled fences
  auto f = findAvailableFence();
  if(f!=nullptr)
    return f;

  // allocate one and add to the pool
  for(uint32_t id=0; id<MaxFences; ++id) {
    auto& i = timeline.timepoint[id];
    if(i!=nullptr)
      continue;

    VkFence fence = VK_NULL_HANDLE;
    try {
      VkFenceCreateInfo fenceInfo = {};
      fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
      fenceInfo.flags = 0;
      vkAssert(vkCreateFence(device.impl,&fenceInfo,nullptr,&fence));

      i = std::make_shared<VFence>(this, fence, uint32_t(id));
      return i;
      }
    catch(...) {
      if(f!=VK_NULL_HANDLE)
        vkDestroyFence(device.impl, fence, nullptr);
      throw;
      }
    }

  //pool is full - wait on one of the fences and reuse it
  waitAny(std::numeric_limits<uint64_t>::max());
  return findAvailableFence();
  }

VkResult VDevice::waitFence(VFence& t, uint64_t timeout) {
  {
    std::lock_guard<std::mutex> guard(timeline.sync);
    if(t.fence==VK_NULL_HANDLE || t.status==VK_SUCCESS)
      return VK_SUCCESS;
    if(timeout==0) {
      t.status = vkGetFenceStatus(device.impl, t.fence);
      if(t.status<=0 && t.status!=VK_NOT_READY)
        return t.status;
      return VK_NOT_READY;
      }
  }

  auto start = Application::tickCount();
  while(true) {
    static const uint64_t toNano = uint64_t(1000*1000);
    uint64_t vkTime = 0;
    if(timeout < std::numeric_limits<uint64_t>::max()/toNano) {
      vkTime = timeout*toNano; // millis to nano convertion
      } else {
      vkTime = std::numeric_limits<uint64_t>::max();
      }

    std::lock_guard<std::mutex> guard(timeline.sync);
    waitAny(vkTime);
    if(t.fence==VK_NULL_HANDLE || t.status==VK_SUCCESS)
      return VK_SUCCESS;
    if(t.status<0 && t.status!=VK_NOT_READY)
      return t.status;

    auto now = Application::tickCount();
    if(now-start > timeout)
      return VK_NOT_READY;
    timeout -= (now-start);
    }
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

std::shared_ptr<VFence> VDevice::submit(VCommandBuffer& cmd) {
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

  auto pfence = aquireFence();
  if(pfence==nullptr)
    throw DeviceLostException();

  VkFence fence = pfence->fence;

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

    std::lock_guard<std::mutex> guard(timeline.sync);
    pfence->status = VK_NOT_READY;
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

    std::lock_guard<std::mutex> guard(timeline.sync);
    pfence->status = VK_NOT_READY;
    graphicsQueue->submit(1,&submitInfo,fence);
    }
  return pfence;
  }


#endif
