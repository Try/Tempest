#include "vdevice.h"

#include "vcommandbuffer.h"
#include "vcommandpool.h"
#include "vfence.h"
#include "vsemaphore.h"
#include "vswapchain.h"

#include <set>

using namespace Tempest::Detail;

static const std::initializer_list<const char*> validationLayers = {
  "VK_LAYER_LUNARG_standard_validation"
  };

static const std::initializer_list<const char*> deviceExtensions = {
  VK_KHR_SWAPCHAIN_EXTENSION_NAME
  };


VDevice::DataHelper::DataHelper(VDevice &owner)
  : cmdPool(owner,VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT),
    cmdBuffer(owner,cmdPool,CmdType::Primary),
    fence(owner),
    graphicsQueue(owner.graphicsQueue){
  }

void VDevice::DataHelper::begin() {
  if(!firstCommit) {
    wait();
    cmdBuffer.reset();
    } else {
    firstCommit=false;
    }
  cmdBuffer.begin(VCommandBuffer::ONE_TIME_SUBMIT_BIT);
  }

void VDevice::DataHelper::end() {
  cmdBuffer.end();

  VkSubmitInfo submitInfo = {};
  submitInfo.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers    = &cmdBuffer.impl;

  fence.reset();
  hasToWait=true;
  vkQueueSubmit(graphicsQueue, 1, &submitInfo, fence.impl);
  }

void VDevice::DataHelper::wait() {
  if(hasToWait)  {
    hasToWait=false;
    fence.wait();
    }
  }

VDevice::VDevice(VulkanApi &api, void *hwnd)
  :instance(api.instance)  {
  createSurface(api,HWND(hwnd));

  uint32_t deviceCount = 0;
  vkEnumeratePhysicalDevices(api.instance, &deviceCount, nullptr);

  if(deviceCount==0)
    throw std::system_error(Tempest::GraphicsErrc::NoDevice);

  std::vector<VkPhysicalDevice> devices(deviceCount);
  vkEnumeratePhysicalDevices(api.instance, &deviceCount, devices.data());

  for(const auto& device:devices)
    if(isDeviceSuitable(device)){
      physicalDevice = device;
      break;
      }

  if(physicalDevice==nullptr)
    throw std::system_error(Tempest::GraphicsErrc::NoDevice);
  createLogicalDevice(api);
  vkGetPhysicalDeviceMemoryProperties(physicalDevice,&memoryProperties);

  getCaps(caps);

  allocator.setDevice(*this);
  data.reset(new DataHelper(*this));
  }

VDevice::~VDevice(){
  data.reset();
  allocator.freeLast();
  vkDeviceWaitIdle(device);
  vkDestroySurfaceKHR(instance,surface,nullptr);
  vkDestroyDevice(device,nullptr);
  }

VkResult VDevice::nextImg(VSwapchain& sw,uint32_t& imageId,VSemaphore& onReady) {
  return vkAcquireNextImageKHR(device,
                               sw.swapChain,
                               std::numeric_limits<uint64_t>::max(),
                               onReady.impl,
                               VK_NULL_HANDLE,
                               &imageId);
  }

void VDevice::createSurface(VulkanApi &api,void* hwnd) {
  auto connection=GetModuleHandle(nullptr);

  VkWin32SurfaceCreateInfoKHR createInfo={};
  createInfo.sType     = VkStructureType::VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
  createInfo.hinstance = connection;
  createInfo.hwnd      = HWND(hwnd);

  if(vkCreateWin32SurfaceKHR(api.instance,&createInfo,nullptr,&surface)!=VK_SUCCESS)
    throw std::system_error(Tempest::GraphicsErrc::NoDevice);
  }

bool VDevice::isDeviceSuitable(VkPhysicalDevice device) {
  QueueFamilyIndices indices = findQueueFamilies(device);
  bool extensionsSupported   = checkDeviceExtensionSupport(device);

  bool swapChainAdequate = false;
  if(extensionsSupported) {
    SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device);
    swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
    }

  return indices.isComplete() && extensionsSupported && swapChainAdequate;
  }

VDevice::QueueFamilyIndices VDevice::findQueueFamilies(VkPhysicalDevice device) {
  QueueFamilyIndices indices;

  uint32_t queueFamilyCount = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

  std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
  vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

  uint32_t i = 0;
  for(const auto& queueFamily : queueFamilies) {
    if(queueFamily.queueCount>0 && queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
      indices.graphicsFamily = i;

    VkBool32 presentSupport=false;
    vkGetPhysicalDeviceSurfaceSupportKHR(device,i,surface,&presentSupport);

    if(queueFamily.queueCount>0 && presentSupport)
      indices.presentFamily = i;

    if(indices.isComplete())
      break;
    i++;
    }
  return indices;
  }

bool VDevice::checkDeviceExtensionSupport(VkPhysicalDevice device) {
  uint32_t extensionCount;
  vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

  std::vector<VkExtensionProperties> availableExtensions(extensionCount);
  vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

  std::set<std::string> requiredExtensions(deviceExtensions.begin(),deviceExtensions.end());

  for(const auto& extension : availableExtensions)
    requiredExtensions.erase(extension.extensionName);

  return requiredExtensions.empty();
  }

VDevice::SwapChainSupportDetails VDevice::querySwapChainSupport(VkPhysicalDevice device) {
  SwapChainSupportDetails details;

  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

  uint32_t formatCount;
  vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);

  if(formatCount != 0){
    details.formats.resize(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
    }

  uint32_t presentModeCount;
  vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);

  if(presentModeCount != 0){
    details.presentModes.resize(presentModeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
    }

  return details;
  }

void VDevice::createLogicalDevice(VulkanApi& api) {
  QueueFamilyIndices indices = findQueueFamilies(physicalDevice);

  std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
  std::set<uint32_t> uniqueQueueFamilies = {indices.graphicsFamily, indices.presentFamily};

  float queuePriority = 1.0f;
  for (uint32_t queueFamily : uniqueQueueFamilies) {
    VkDeviceQueueCreateInfo queueCreateInfo = {};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = queueFamily;
    queueCreateInfo.queueCount = 1;
    queueCreateInfo.pQueuePriorities = &queuePriority;
    queueCreateInfos.push_back(queueCreateInfo);
    }

  VkPhysicalDeviceFeatures deviceFeatures = {};

  VkDeviceCreateInfo createInfo = {};
  createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

  createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
  createInfo.pQueueCreateInfos = queueCreateInfos.data();

  createInfo.pEnabledFeatures = &deviceFeatures;

  createInfo.enabledExtensionCount   = static_cast<uint32_t>(deviceExtensions.size());
  createInfo.ppEnabledExtensionNames = deviceExtensions.begin();

  if( api.validation ) {
    createInfo.enabledLayerCount   = static_cast<uint32_t>(validationLayers.size());
    createInfo.ppEnabledLayerNames = validationLayers.begin();
    } else {
    createInfo.enabledLayerCount = 0;
    }

  if(vkCreateDevice(physicalDevice, &createInfo, nullptr, &device)!=VK_SUCCESS)
    throw std::system_error(Tempest::GraphicsErrc::NoDevice);

  vkGetDeviceQueue(device, indices.graphicsFamily, 0, &graphicsQueue);
  vkGetDeviceQueue(device, indices.presentFamily,  0, &presentQueue);
  }

uint32_t VDevice::memoryTypeIndex(uint32_t typeBits,VkMemoryPropertyFlags props) const {
  for(size_t i=0; i<memoryProperties.memoryTypeCount; ++i) {
    auto bit = (uint32_t(1) << i);
    if((typeBits & bit)!=0) {
      if((memoryProperties.memoryTypes[i].propertyFlags & props)==props)
        return i;
      }
    }
  throw std::runtime_error("failed to get correct memory type");
  }

void VDevice::getCaps(AbstractGraphicsApi::Caps &c) {
  /*
   * formats support table: https://vulkan.lunarg.com/doc/view/1.0.30.0/linux/vkspec.chunked/ch31s03.html
   */
  VkFormatFeatureFlags imageRqFlags = VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT|VK_FORMAT_FEATURE_BLIT_DST_BIT|VK_FORMAT_FEATURE_BLIT_SRC_BIT;
  VkFormatProperties frm={};
  vkGetPhysicalDeviceFormatProperties(physicalDevice,VK_FORMAT_R8G8B8_UNORM,&frm);
  c.rgb8  = (frm.optimalTilingFeatures & imageRqFlags) &&
            (frm.linearTilingFeatures  & imageRqFlags);

  vkGetPhysicalDeviceFormatProperties(physicalDevice,VK_FORMAT_R8G8B8A8_UNORM,&frm); // must-have
  c.rgba8 = (frm.optimalTilingFeatures & imageRqFlags);

  VkPhysicalDeviceProperties limits;
  vkGetPhysicalDeviceProperties(physicalDevice,&limits);
  c.minUboAligment = size_t(limits.limits.minUniformBufferOffsetAlignment);
  }

VkResult VDevice::present(VSwapchain &sw, const VSemaphore *wait, size_t wSize, uint32_t imageId) {
  VkPresentInfoKHR presentInfo = {};
  presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

  presentInfo.waitSemaphoreCount = wSize;
  presentInfo.pWaitSemaphores    = &wait->impl;

  VkSwapchainKHR swapChains[] = {sw.swapChain};
  presentInfo.swapchainCount  = 1;
  presentInfo.pSwapchains     = swapChains;
  presentInfo.pImageIndices   = &imageId;

  return vkQueuePresentKHR(presentQueue,&presentInfo);
  }

void VDevice::draw(VCommandBuffer& cmd, VSemaphore &wait, VSemaphore &onReady, VFence &onReadyCpu) {
  VkSubmitInfo submitInfo = {};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

  VkSemaphore          waitSemaphores[] = {wait.impl};
  VkPipelineStageFlags waitStages[]     = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
  submitInfo.waitSemaphoreCount = 1;
  submitInfo.pWaitSemaphores   = waitSemaphores;
  submitInfo.pWaitDstStageMask = waitStages;

  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers    = &cmd.impl;

  submitInfo.signalSemaphoreCount = 1;
  submitInfo.pSignalSemaphores    = &onReady.impl;

  onReadyCpu.reset();
  vkAssert(vkQueueSubmit(graphicsQueue,1,&submitInfo,onReadyCpu.impl));
  }

void VDevice::copy(VBuffer &dest, const VBuffer &src,size_t size) {
  data->begin();
  data->cmdBuffer.copy(dest,0,src,0,size);
  data->end();
  }

void VDevice::copy(VTexture &dest, uint32_t w, uint32_t h, const VBuffer &src) {
  data->begin();
  data->cmdBuffer.copy(dest,w,h,src);
  data->end();
  }

void VDevice::changeLayout(VTexture &dest, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipCount) {
  data->begin();
  data->cmdBuffer.changeLayout(dest,oldLayout,newLayout,mipCount);
  data->end();
  }

void VDevice::generateMipmap(VTexture& image, VkFormat frm, uint32_t texWidth, uint32_t texHeight, uint32_t mipLevels) {
  data->begin();
  data->cmdBuffer.generateMipmap(image,physicalDevice,frm,texWidth,texHeight,mipLevels);
  data->end();
  }

void VDevice::waitData() {
  data->wait();
  }

