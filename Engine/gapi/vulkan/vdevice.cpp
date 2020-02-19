#include "vdevice.h"

#include "vcommandbuffer.h"
#include "vcommandpool.h"
#include "vfence.h"
#include "vsemaphore.h"
#include "vswapchain.h"
#include "vbuffer.h"
#include "vtexture.h"
#include "system/api/x11api.h"

#include <Tempest/Log>
#include <thread>
#include <set>

#ifdef __LINUX__
#define VK_USE_PLATFORM_XLIB_KHR
#include <X11/Xlib.h>
#include <vulkan/vulkan_xlib.h>
#undef Always
#endif

using namespace Tempest;
using namespace Tempest::Detail;

static const std::initializer_list<const char*> requiredExtensions = {
  VK_KHR_SWAPCHAIN_EXTENSION_NAME
  };

class VDevice::FakeWindow final {
  public:
    FakeWindow(VDevice& dev)
      :instance(dev.instance) {
      w = SystemApi::createWindow(nullptr,SystemApi::Hidden);
      }
    ~FakeWindow() {
      if(surface!=VK_NULL_HANDLE)
        vkDestroySurfaceKHR(instance,surface,nullptr);
      if(w!=nullptr)
        SystemApi::destroyWindow(w);
      }

    VkInstance         instance = nullptr;
    SystemApi::Window* w        = nullptr;
    VkSurfaceKHR       surface  = VK_NULL_HANDLE;
  };

VDevice::DataStream::DataStream(VDevice &owner)
  : owner(owner),
    cmdPool(owner,VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT),
    cmdBuffer(owner,cmdPool,nullptr,CmdType::Primary),
    fence(owner),
    gpuQueue(owner.graphicsQueue){
  hold.reserve(32);
  }

VDevice::DataStream::~DataStream() {
  wait();
  }

void VDevice::DataStream::begin() {
  cmdBuffer.begin(VCommandBuffer::ONE_TIME_SUBMIT_BIT);
  }

void VDevice::DataStream::end() {
  VkSubmitInfo submitInfo = {};
  submitInfo.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers    = &cmdBuffer.impl;

  cmdBuffer.end();
  fence.reset();
  owner.graphicsQueue->submit(1,&submitInfo,fence.impl);

  state.store(StWait);
  }

void VDevice::DataStream::wait() {
  while(true) {
    auto s = state.load();
    if(s==StIdle)
      return;
    if(s==StWait)  {
      fence.wait();
      cmdBuffer.reset();
      hold.clear();
      state.store(StIdle);
      return;
      }
    }
  }

VDevice::VDevice(VulkanApi &api, const char* gpuName)
  :instance(api.instance)  {

  uint32_t deviceCount = 0;
  vkEnumeratePhysicalDevices(api.instance, &deviceCount, nullptr);

  if(deviceCount==0)
    throw std::system_error(Tempest::GraphicsErrc::NoDevice);

  std::vector<VkPhysicalDevice> devices(deviceCount);
  vkEnumeratePhysicalDevices(api.instance, &deviceCount, devices.data());
  //std::swap(devices[0],devices[1]);

  FakeWindow fakeWnd{*this};
  fakeWnd.surface = createSurface(fakeWnd.w);

  for(const auto& device:devices) {
    if(isDeviceSuitable(device,fakeWnd.surface,gpuName)) {
      implInit(device,fakeWnd.surface);
      return;
      }
    }

  throw std::system_error(Tempest::GraphicsErrc::NoDevice);
  }

VDevice::~VDevice(){
  vkDeviceWaitIdle(device);
  data.reset();
  allocator.freeLast();
  vkDestroyDevice(device,nullptr);
  }

void VDevice::implInit(VkPhysicalDevice pdev, VkSurfaceKHR surf) {
  Detail::VulkanApi::getDeviceProps(pdev,props);
  deviceQueueProps(props,pdev,surf);

  createLogicalDevice(pdev);
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
  auto connection=GetModuleHandle(nullptr);

  VkWin32SurfaceCreateInfoKHR createInfo={};
  createInfo.sType     = VkStructureType::VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
  createInfo.hinstance = connection;
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

bool VDevice::isDeviceSuitable(VkPhysicalDevice device, VkSurfaceKHR surf, const char* gpuName) {
  VkProps prop = {};
  if(gpuName!=nullptr) {
    Detail::VulkanApi::getDeviceProps(device,props);
    if(std::strcmp(gpuName,props.name)!=0)
      return false;
    }
  deviceQueueProps(prop,device,surf);
  bool    extensionsSupported = checkDeviceExtensionSupport(device);

  bool swapChainAdequate = false;
  if(extensionsSupported && surf!=VK_NULL_HANDLE) {
    auto swapChainSupport = querySwapChainSupport(device,surf);
    swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
    }

  // TODO: device selection api
  return extensionsSupported &&
         prop.graphicsFamily!=uint32_t(-1) &&
         (prop.presentFamily!=uint32_t(-1) || surf==VK_NULL_HANDLE) &&
         (swapChainAdequate || surf==VK_NULL_HANDLE);
  }

void VDevice::deviceQueueProps(Detail::VulkanApi::VkProp& prop,VkPhysicalDevice device, VkSurfaceKHR surf) {
  uint32_t queueFamilyCount = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

  std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
  vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

  uint32_t graphics = uint32_t(-1);
  uint32_t present  = uint32_t(-1);

  for(size_t i=0;i<queueFamilyCount;++i) {
    const auto& queueFamily = queueFamilies[i];
    if(queueFamily.queueCount<=0)
      continue;

    if(queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT)
      graphics = i;

    VkBool32 presentSupport=false;
    if(surf!=VK_NULL_HANDLE)
      vkGetPhysicalDeviceSurfaceSupportKHR(device,i,surf,&presentSupport);

    if(presentSupport)
      present = i;
    }

  VkPhysicalDeviceProperties p={};
  vkGetPhysicalDeviceProperties(device,&p);

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
  uint32_t extensionCount;
  vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

  std::vector<VkExtensionProperties> ext(extensionCount);
  vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, ext.data());

  return ext;
  }

bool VDevice::checkForExt(const std::vector<VkExtensionProperties>& list, const char* name) {
  for(auto& r:list)
    if(std::strcmp(name,r.extensionName)==0)
      return true;
  return false;
  }

VDevice::SwapChainSupport VDevice::querySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface) {
  SwapChainSupport details;

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

void VDevice::createLogicalDevice(VkPhysicalDevice pdev) {
  auto ext = extensionsList(pdev);

  std::vector<const char*> rqExt = requiredExtensions;
  if(checkForExt(ext,VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME)) {
    props.hasMemRq2 = true;
    rqExt.push_back(VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME);
    }
  if(checkForExt(ext,VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME)) {
    props.hasDedicatedAlloc = true;
    rqExt.push_back(VK_KHR_DEDICATED_ALLOCATION_EXTENSION_NAME);
    }

  std::array<uint32_t,2> uniqueQueueFamilies = {props.graphicsFamily, props.presentFamily};
  float  queuePriority = 1.0f;
  size_t queueCnt      = 0;
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

  VkPhysicalDeviceFeatures supportedFeatures={};
  vkGetPhysicalDeviceFeatures(pdev,&supportedFeatures);

  VkPhysicalDeviceFeatures deviceFeatures = {};
  deviceFeatures.samplerAnisotropy    = supportedFeatures.samplerAnisotropy;
  deviceFeatures.textureCompressionBC = supportedFeatures.textureCompressionBC;

  VkDeviceCreateInfo createInfo = {};
  createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

  createInfo.queueCreateInfoCount = uint32_t(queueCnt);
  createInfo.pQueueCreateInfos    = &qinfo[0];
  createInfo.pEnabledFeatures     = &deviceFeatures;

  createInfo.enabledExtensionCount   = static_cast<uint32_t>(rqExt.size());
  createInfo.ppEnabledExtensionNames = rqExt.data();

  if(vkCreateDevice(pdev, &createInfo, nullptr, &device)!=VK_SUCCESS)
    throw std::system_error(Tempest::GraphicsErrc::NoDevice);

  for(size_t i=0;i<queueCnt;++i) {
    vkGetDeviceQueue(device, queues[i].family, 0, &queues[i].impl);
    if(queues[i].family==props.graphicsFamily)
      graphicsQueue = &queues[i];
    if(queues[i].family==props.presentFamily)
      presentQueue = &queues[i];
    }

  if(props.hasMemRq2) {
    vkGetBufferMemoryRequirements2 = reinterpret_cast<PFN_vkGetBufferMemoryRequirements2KHR>
        (vkGetDeviceProcAddr(device,"vkGetBufferMemoryRequirements2KHR"));
    vkGetImageMemoryRequirements2 = reinterpret_cast<PFN_vkGetImageMemoryRequirements2KHR>
        (vkGetDeviceProcAddr(device,"vkGetImageMemoryRequirements2KHR"));
    }
  }

VDevice::MemIndex VDevice::memoryTypeIndex(uint32_t typeBits, VkMemoryPropertyFlags props, VkImageTiling tiling) const {
  for(size_t i=0; i<memoryProperties.memoryTypeCount; ++i) {
    auto bit = (uint32_t(1) << i);
    if((typeBits & bit)!=0) {
      if((memoryProperties.memoryTypes[i].propertyFlags & props)==props) {
        MemIndex ret;
        ret.typeId = uint32_t(i);
        // avoid bufferImageGranularity shenanigans
        ret.headId = (tiling==VK_IMAGE_TILING_OPTIMAL && this->props.bufferImageGranularity>1) ? ret.typeId*2+1 : ret.typeId*2+0;
        return ret;
        }
      }
    }
  throw std::runtime_error("failed to get correct memory type");
  }

VkResult VDevice::present(VSwapchain &sw, const VSemaphore *wait, size_t wSize, uint32_t imageId) {
  VkPresentInfoKHR presentInfo = {};
  presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

  presentInfo.waitSemaphoreCount = uint32_t(wSize);
  presentInfo.pWaitSemaphores    = &wait->impl;

  VkSwapchainKHR swapChains[] = {sw.swapChain};
  presentInfo.swapchainCount  = 1;
  presentInfo.pSwapchains     = swapChains;
  presentInfo.pImageIndices   = &imageId;

  return presentQueue->present(presentInfo);
  }

void VDevice::waitData() {
  data->wait();
  }

const char *VDevice::renderer() const {
  return props.name;
  }

void VDevice::waitIdle() const {
  vkDeviceWaitIdle(device);
  }


VDevice::Data::Data(VDevice &dev)
  :sync(dev.allocSync),stream(dev.data->get()) {
  physicalDevice = dev.physicalDevice;
  }

VDevice::Data::~Data() noexcept(false) {
  try {
    commit();
    }
  catch(...) {
    std::hash<std::thread::id> h;
    Tempest::Log::e("data queue commit failed at thread[",h(std::this_thread::get_id()),"]");
    throw;
    }
  }

void VDevice::Data::flush(const VBuffer &src, size_t size) {
  if(commited){
    stream.begin();
    commited=false;
    }
  stream.cmdBuffer.flush(src,size);
  }

void VDevice::Data::copy(VBuffer &dest, const VBuffer &src, size_t size) {
  if(commited){
    stream.begin();
    commited=false;
    }
  stream.cmdBuffer.copy(dest,0,src,0,size);
  }

void VDevice::Data::copy(VTexture &dest, uint32_t w, uint32_t h, uint32_t mip, const VBuffer &src, size_t offset) {
  if(commited){
    stream.begin();
    commited=false;
    }
  stream.cmdBuffer.copy(dest,w,h,mip,src,offset);
  }

void VDevice::Data::copy(VBuffer &dest, uint32_t w, uint32_t h, uint32_t mip, const VTexture &src, size_t offset) {
  if(commited){
    stream.begin();
    commited=false;
    }
  stream.cmdBuffer.copy(dest,w,h,mip,src,offset);
  }

void VDevice::Data::changeLayout(VTexture &dest, VkFormat frm, VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipCount) {
  if(commited){
    stream.begin();
    commited=false;
    }
  stream.cmdBuffer.changeLayout(dest.impl,frm,oldLayout,newLayout,mipCount,false);
  }

void VDevice::Data::generateMipmap(VTexture &image, VkFormat frm, uint32_t texWidth, uint32_t texHeight, uint32_t mipLevels) {
  if(commited){
    stream.begin();
    commited=false;
    }
  stream.cmdBuffer.generateMipmap(image,physicalDevice,frm,texWidth,texHeight,mipLevels);
  }

void VDevice::Data::hold(BufPtr &b) {
  if(commited){
    stream.begin();
    commited=false;
    }
  stream.hold.emplace_back(ResPtr(b.handler));
  }

void VDevice::Data::hold(TexPtr &b) {
  if(commited){
    stream.begin();
    commited=false;
    }
  stream.hold.emplace_back(ResPtr(b.handler));
  }

void VDevice::Data::commit() {
  if(commited)
    return;
  commited=true;
  stream.end();
  }

void VDevice::Queue::submit(uint32_t submitCount, const VkSubmitInfo* pSubmits, VkFence fence) {
  std::lock_guard<std::mutex> guard(sync);
  vkAssert(vkQueueSubmit(impl,submitCount,pSubmits,fence));
  }

VkResult VDevice::Queue::present(VkPresentInfoKHR& presentInfo) {
  std::lock_guard<std::mutex> guard(sync);
  return vkQueuePresentKHR(impl,&presentInfo);
  }

VDevice::DataMgr::DataMgr(VDevice& dev) {
  for(auto& i:streams)
    i.reset(new DataStream(dev));
  }

VDevice::DataMgr::~DataMgr() {
  wait();
  }

VDevice::DataStream& VDevice::DataMgr::get() {
  auto id = at.fetch_add(1)%2;

  std::lock_guard<SpinLock> guard(sync[id]);
  auto& st = *streams[id];
  st.wait();
  st.state.store(StRecording);
  return st;
  }

void VDevice::DataMgr::wait() {
  for(size_t i=0;i<size;++i) {
    std::lock_guard<SpinLock> guard(sync[i]);
    streams[i]->wait();
    }
  }
