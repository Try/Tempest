#if defined(TEMPEST_BUILD_VULKAN)

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

bool VulkanInstance::VkProp::hasFilteredFormat(TextureFormat f) const {
  uint64_t  m = uint64_t(1) << uint64_t(f);
  return (filteredLinearFormat&m)!=0;
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
    VkProp prop = {};
    devicePropsShort(devices[i],prop);
    devList[i] = static_cast<Tempest::AbstractGraphicsApi::Props&>(prop);
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

  VkPhysicalDeviceMeshShaderPropertiesNV propMesh = {};
  propMesh.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_PROPERTIES_NV;

  VkPhysicalDeviceProperties prop={};
  vkGetPhysicalDeviceProperties(physicalDevice,&prop);
  c.nonCoherentAtomSize = size_t(prop.limits.nonCoherentAtomSize);
  if(c.nonCoherentAtomSize==0)
    c.nonCoherentAtomSize=1;

  c.bufferImageGranularity = size_t(prop.limits.bufferImageGranularity);
  if(c.bufferImageGranularity==0)
    c.bufferImageGranularity=1;
  }

void VulkanInstance::devicePropsShort(VkPhysicalDevice physicalDevice, VkProp& props) const {
  /*
   * formats support table: https://vulkan.lunarg.com/doc/view/1.0.30.0/linux/vkspec.chunked/ch31s03.html
   */
  VkFormatFeatureFlags imageRqFlags    = VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT|VK_FORMAT_FEATURE_BLIT_DST_BIT|VK_FORMAT_FEATURE_BLIT_SRC_BIT;
  VkFormatFeatureFlags imageRqFlagsBC  = VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;
  VkFormatFeatureFlags attachRqFlags   = VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT|VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT;
  VkFormatFeatureFlags depthAttFlags   = VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;
  VkFormatFeatureFlags storageAttFlags = VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT;
  VkFormatFeatureFlags atomicFlags     = VK_FORMAT_FEATURE_STORAGE_IMAGE_ATOMIC_BIT;

  auto ext = extensionsList(physicalDevice);
  if(checkForExt(ext,VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME))
    props.hasMemRq2 = true;
  if(checkForExt(ext,VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME))
    props.hasDedicatedAlloc = true;
  if(hasDeviceFeatures2 && checkForExt(ext,VK_KHR_SYNCHRONIZATION_2_EXTENSION_NAME))
    ;//props.hasSync2 = true;
  if(hasDeviceFeatures2 && checkForExt(ext,VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME))
    props.hasDeviceAddress = true;
  if(hasDeviceFeatures2 && checkForExt(ext,VK_KHR_SPIRV_1_4_EXTENSION_NAME)) {
    props.hasSpirv_1_4 = true;
    }
  if(hasDeviceFeatures2 &&
     checkForExt(ext,VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME) &&
     checkForExt(ext,VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME) &&
     checkForExt(ext,VK_KHR_RAY_QUERY_EXTENSION_NAME)) {
    props.raytracing.rayQuery = true;
    }
  if(hasDeviceFeatures2 && checkForExt(ext,VK_EXT_MESH_SHADER_EXTENSION_NAME)) {
    props.meshlets.meshShader = true;
    }
  if(hasDeviceFeatures2 && checkForExt(ext,VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME)) {
    props.hasDescIndexing = true;
    }
  if(hasDeviceFeatures2 && checkForExt(ext,VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME)) {
    props.hasDynRendering = true;
    }
  if(hasDeviceFeatures2 && checkForExt(ext,VK_KHR_FRAGMENT_SHADER_BARYCENTRIC_EXTENSION_NAME)) {
    props.hasBarycentrics = true;
    }
  if(hasDeviceFeatures2 && checkForExt(ext,VK_EXT_ROBUSTNESS_2_EXTENSION_NAME)) {
    props.hasRobustness2 = true;
    }
  if(checkForExt(ext,VK_EXT_DEBUG_MARKER_EXTENSION_NAME)) {
    props.hasDebugMarker = true;
    }
  if(checkForExt(ext,VK_EXT_LOAD_STORE_OP_NONE_EXTENSION_NAME)) {
    props.hasStoreOpNone = true;
    }
  if(checkForExt(ext,VK_KHR_MAINTENANCE_1_EXTENSION_NAME)) {
    props.hasMaintenance1 = true;
    }
  if(checkForExt(ext,VK_KHR_VULKAN_MEMORY_MODEL_EXTENSION_NAME)) {
    props.memoryModel = true;
    }

  VkPhysicalDeviceProperties devP={};
  vkGetPhysicalDeviceProperties(physicalDevice,&devP);

  VkPhysicalDeviceFeatures supportedFeatures={};
  vkGetPhysicalDeviceFeatures(physicalDevice,&supportedFeatures);

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

    auto vkGetPhysicalDeviceFeatures2   = PFN_vkGetPhysicalDeviceFeatures2  (vkGetInstanceProcAddr(instance,"vkGetPhysicalDeviceFeatures2KHR"));
    auto vkGetPhysicalDeviceProperties2 = PFN_vkGetPhysicalDeviceProperties2(vkGetInstanceProcAddr(instance,"vkGetPhysicalDeviceProperties2KHR"));

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
    if(!props.meshlets.meshShader && props.hasSpirv_1_4 && false) {
      props.meshlets.meshShaderEmulated = true;
      if(props.meshlets.meshShaderEmulated) {
        props.meshlets.maxGroups.x      = devP.limits.maxComputeWorkGroupCount[0];
        props.meshlets.maxGroups.y      = devP.limits.maxComputeWorkGroupCount[1];
        props.meshlets.maxGroups.z      = devP.limits.maxComputeWorkGroupCount[2];
        props.meshlets.maxGroupSize.x   = devP.limits.maxComputeWorkGroupSize [0];
        props.meshlets.maxGroupSize.y   = devP.limits.maxComputeWorkGroupSize [1];
        props.meshlets.maxGroupSize.z   = devP.limits.maxComputeWorkGroupSize [2];
        }
      }

    if(indexingFeatures.runtimeDescriptorArray!=VK_FALSE) {
      // NOTE1: no UBO support - won't do
      // NOTE2: no Storage-ismage support on Intel
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

  props.ssbo.offsetAlign  = size_t(devP.limits.minUniformBufferOffsetAlignment);
  props.ssbo.maxRange     = size_t(devP.limits.maxStorageBufferRange);

  props.ubo.offsetAlign   = size_t(devP.limits.minUniformBufferOffsetAlignment);
  props.ubo.maxRange      = size_t(devP.limits.maxUniformBufferRange);

  props.push.maxRange     = size_t(devP.limits.maxPushConstantsSize);

  props.anisotropy        = supportedFeatures.samplerAnisotropy;
  props.maxAnisotropy     = devP.limits.maxSamplerAnisotropy;
  props.tesselationShader = supportedFeatures.tessellationShader;
  props.geometryShader    = supportedFeatures.geometryShader;

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

  uint64_t smpFormat=0, attFormat=0, dattFormat=0, storFormat=0, atomFormat=0, filteredLinearFormat=0;
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
