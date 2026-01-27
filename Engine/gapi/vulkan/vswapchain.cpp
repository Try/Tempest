#if defined(TEMPEST_BUILD_VULKAN)

#include "vswapchain.h"

#include <Tempest/Application>
#include <Tempest/SystemApi>
#include <Tempest/Platform>

#include "vdevice.h"

#if defined(__UNIX__)
#include "system/api/x11api.h"
#endif

#if defined(__WINDOWS__)
#  define VK_USE_PLATFORM_WIN32_KHR
#  include <windows.h>
#  include <vulkan/vulkan_win32.h>
#elif defined(__ANDROID__)
#  define VK_USE_PLATFORM_ANDROID_KHR
#  include <android/native_window.h>
#  include <vulkan/vulkan_android.h>
extern "C" ANativeWindow* tempest_android_get_native_window();
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

static VkResult VKAPI_CALL vkxRevertFence(VkDevice device, VkFence* pFence) {
  // HACK: revert fence state to signaled
  VkFenceCreateInfo fenceInfo = {};
  fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

  vkDestroyFence(device, *pFence, nullptr);
  *pFence = VK_NULL_HANDLE;
  return vkCreateFence(device,&fenceInfo,nullptr,pFence);
  }

static VkResult vkAcquireNextImageDBG(
    VkDevice                                    device,
    VkSwapchainKHR                              swapchain,
    uint64_t                                    timeout,
    VkSemaphore                                 semaphore,
    VkFence                                     fence,
    uint32_t*                                   pImageIndex) {
#if 1
  return  vkAcquireNextImageKHR(device, swapchain, timeout, semaphore, fence, pImageIndex);
#else
  static std::atomic_uint32_t cnt = {};
  auto id = cnt.fetch_add(1);
  if(id==0)
    return VK_ERROR_OUT_OF_DATE_KHR;

  return  vkAcquireNextImageKHR(device, swapchain, timeout, semaphore, fence, pImageIndex);
#endif
  }


VSwapchain::FenceList::FenceList(VkDevice dev, uint32_t cnt)
  :dev(dev), size(0) {
  data.reset(new VkFence[cnt]);
  for(uint32_t i=0; i<cnt; ++i)
    data[i] = VK_NULL_HANDLE;

  try {
    VkFenceCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    info.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    for(uint32_t i=0; i<cnt; ++i) {
      vkAssert(vkCreateFence(dev,&info,nullptr,&data[i]));
      ++size;
      }
    }
  catch(...) {
    for(uint32_t i=0; i<cnt; ++i) {
      if(data[i]!=VK_NULL_HANDLE)
        vkDestroyFence(dev,data[i],nullptr);
      }
    throw;
    }
  }

VSwapchain::FenceList::FenceList(VSwapchain::FenceList&& oth)
  :dev(oth.dev), size(oth.size) {
  data     = std::move(oth.data);
  oth.size = 0;
  }

VSwapchain::FenceList& VSwapchain::FenceList::operator =(VSwapchain::FenceList&& oth) {
  std::swap(dev,  oth.dev);
  std::swap(data, oth.data);
  std::swap(size, oth.size);
  return *this;
  }

VSwapchain::FenceList::~FenceList() {
  for(uint32_t i=0; i<size; ++i)
    vkDestroyFence(dev,data[i],nullptr);
  }

void VSwapchain::FenceList::waitAll() {
  vkWaitForFences(dev, size, data.get(), VK_TRUE,std::numeric_limits<uint64_t>::max());
  }


VSwapchain::SemaphoreList::SemaphoreList(VkDevice dev, uint32_t cnt)
  :dev(dev), size(0) {
  data.reset(new VkSemaphore[cnt]);
  for(uint32_t i=0; i<cnt; ++i)
    data[i] = VK_NULL_HANDLE;

  try {
    VkSemaphoreCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    info.flags = 0;
    for(uint32_t i=0; i<cnt; ++i) {
      vkAssert(vkCreateSemaphore(dev,&info,nullptr,&data[i]));
      ++size;
      }
    }
  catch(...) {
    for(uint32_t i=0; i<cnt; ++i) {
      if(data[i]!=VK_NULL_HANDLE)
        vkDestroySemaphore(dev,data[i],nullptr);
      }
    throw;
    }
  }

VSwapchain::SemaphoreList::SemaphoreList(VSwapchain::SemaphoreList&& oth)
  :dev(oth.dev), size(oth.size) {
  data     = std::move(oth.data);
  oth.size = 0;
  }

VSwapchain::SemaphoreList& VSwapchain::SemaphoreList::operator =(VSwapchain::SemaphoreList&& oth) {
  std::swap(dev,  oth.dev);
  std::swap(data, oth.data);
  std::swap(size, oth.size);
  return *this;
  }

VSwapchain::SemaphoreList::~SemaphoreList() {
  for(uint32_t i=0; i<size; ++i)
    vkDestroySemaphore(dev,data[i],nullptr);
  }

VSwapchain::VSwapchain(VDevice &device, SystemApi::Window* hwnd)
  :device(device), hwnd(hwnd) {
  try {
    surface = createSurface(device.instance, hwnd);
    }
  catch(...) {
    cleanup();
    throw;
    }
  createSwapchain(device);
  }

VSwapchain::~VSwapchain() {
  cleanup();
  }

bool VSwapchain::checkPresentSupport(VkPhysicalDevice device, uint32_t queueFamilyIndex) {
#if defined(__WINDOWS__)
  const bool presentSupport = vkGetPhysicalDeviceWin32PresentationSupportKHR(device, queueFamilyIndex)!=VK_FALSE;
#elif defined(__ANDROID__)
  // Android always supports presentation if Vulkan is available
  const bool presentSupport = true;
#elif defined(__UNIX__)
  bool presentSupport = false;
  if(auto dpy = reinterpret_cast<Display*>(X11Api::display())){
    auto screen   = DefaultScreen(dpy);
    auto visualId = XVisualIDFromVisual(DefaultVisual(dpy,screen));
    presentSupport = vkGetPhysicalDeviceXlibPresentationSupportKHR(device,queueFamilyIndex,dpy,visualId)!=VK_FALSE;
    }
#else
#warning "wsi for vulkan not implemented on this platform"
  const bool presentSupport = false;
#endif
  return presentSupport;
  }

void VSwapchain::cleanupSwapchain() noexcept {
  // aquire is not a 'true' queue operation - have to wait explicitly on it
  aquireFence.waitAll();
  // wait for vkQueuePresent to finish, so we can delete semaphores
  // NOTE: maybe update to VK_KHR_present_wait ?
  device.presentQueue->waitIdle();
  aquireFence  = FenceList();
  presentFence = FenceList();
  aquireSem    = SemaphoreList();
  presentSem   = SemaphoreList();

  for(auto imageView : views)
    if(map!=nullptr && imageView!=VK_NULL_HANDLE)
      map->notifyDestroy(imageView);
  map = nullptr;
  for(auto imageView : views)
    if(imageView!=VK_NULL_HANDLE)
      vkDestroyImageView(device.device.impl,imageView,nullptr);
  views.clear();
  sync.clear();

  images.clear();
  if(swapChain!=VK_NULL_HANDLE)
    vkDestroySwapchainKHR(device.device.impl,swapChain,nullptr);

  swapChain            = VK_NULL_HANDLE;
  swapChainImageFormat = VK_FORMAT_UNDEFINED;
  swapChainExtent      = {};
  }

void VSwapchain::cleanupSurface() noexcept {
  if(surface!=VK_NULL_HANDLE)
    vkDestroySurfaceKHR(device.instance,surface,nullptr);
  surface = VK_NULL_HANDLE;
  }

void VSwapchain::reset() {
  Tempest::Log::i("VSwapchain::reset() called - possible screen rotation detected");
  Rect rect = SystemApi::windowClientRect(hwnd);
  Tempest::Log::i("Window client rect: ", rect.w, "x", rect.h);

  cleanupSwapchain();
  createSwapchain(device);

  Tempest::Log::i("VSwapchain::reset() completed - swapchain recreated");
  }

void VSwapchain::cleanup() noexcept {
  cleanupSwapchain();
  cleanupSurface();
  }

VkSurfaceKHR VSwapchain::createSurface(VkInstance instance, void* hwnd) {
  if(hwnd==nullptr)
    return VK_NULL_HANDLE;
  VkSurfaceKHR ret = VK_NULL_HANDLE;
#ifdef __WINDOWS__
  VkWin32SurfaceCreateInfoKHR createInfo={};
  createInfo.sType     = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
  createInfo.hinstance = GetModuleHandleA(nullptr);
  createInfo.hwnd      = HWND(hwnd);
  if(vkCreateWin32SurfaceKHR(instance,&createInfo,nullptr,&ret)!=VK_SUCCESS)
    throw std::system_error(Tempest::GraphicsErrc::NoDevice);
#elif defined(__ANDROID__)
  ANativeWindow* window = tempest_android_get_native_window();
  if(window==nullptr)
    throw std::system_error(Tempest::GraphicsErrc::NoDevice);
  VkAndroidSurfaceCreateInfoKHR createInfo = {};
  createInfo.sType  = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
  createInfo.window = window;
  if(vkCreateAndroidSurfaceKHR(instance, &createInfo, nullptr, &ret)!=VK_SUCCESS)
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

void VSwapchain::createSwapchain(VDevice& device) {
  for(uint32_t attempt=0; ; ++attempt) {
    if(attempt>1) {
      // Window keep changing due to external factor (user resize, KDE animations, etc) - slow down now
      std::this_thread::yield();
      }
    Rect rect = SystemApi::windowClientRect(hwnd);
    if(rect.isEmpty())
      return;

    try {
      auto     support  = device.querySwapChainSupport(surface);
      uint32_t imgCount = findImageCount(support);
      auto     code     = createSwapchain(device,support,rect,imgCount);
      if(code==VK_ERROR_OUT_OF_DATE_KHR || code==VK_SUBOPTIMAL_KHR) {
        cleanupSwapchain();
        continue;
        }
      break;
      }
    catch(...) {
      cleanupSwapchain();
      throw;
      }
    }
  }

VkResult VSwapchain::createSwapchain(VDevice& device, const SwapChainSupport& swapChainSupport,
                                     const Rect& rect, uint32_t imgCount) {
  VkBool32 support=false;
  vkGetPhysicalDeviceSurfaceSupportKHR(device.physicalDevice,device.presentQueue->family,surface,&support);
  if(!support)
    throw std::system_error(Tempest::GraphicsErrc::NoDevice);

  VkSurfaceFormatKHR surfaceFormat = findSwapSurfaceFormat(swapChainSupport.formats);
  VkPresentModeKHR   presentMode   = findSwapPresentMode  (swapChainSupport.presentModes);
  VkExtent2D         extent        = findSwapExtent       (swapChainSupport.capabilities,uint32_t(rect.w),uint32_t(rect.h));

  VkSwapchainCreateInfoKHR createInfo = {};
  createInfo.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  createInfo.surface          = surface;
  createInfo.minImageCount    = imgCount;
  createInfo.imageFormat      = surfaceFormat.format;
  createInfo.imageColorSpace  = surfaceFormat.colorSpace;
  createInfo.imageExtent      = extent;
  createInfo.imageArrayLayers = 1;
  createInfo.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
  createInfo.imageUsage      &= swapChainSupport.capabilities.supportedUsageFlags;

  if(device.graphicsQueue!=device.presentQueue) {
    const uint32_t qidx[]  = {device.graphicsQueue->family,device.presentQueue->family};
    createInfo.imageSharingMode      = VK_SHARING_MODE_CONCURRENT;
    createInfo.queueFamilyIndexCount = 2;
    createInfo.pQueueFamilyIndices   = qidx;
    } else {
    createInfo.imageSharingMode      = VK_SHARING_MODE_EXCLUSIVE;
    }

  // Determine the correct transform based on window dimensions for Android
  VkSurfaceTransformFlagBitsKHR selectedTransform = swapChainSupport.capabilities.currentTransform;

#if defined(__ANDROID__)
  // On Android, manually determine transform from window dimensions if driver doesn't report correctly
  bool isLandscape = (extent.width > extent.height);

  // Check if currentTransform makes sense for current orientation
  bool needManualTransform = false;
  if(isLandscape) {
    // For landscape, we expect either IDENTITY or ROTATE_270
    if(swapChainSupport.capabilities.currentTransform != VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR &&
       swapChainSupport.capabilities.currentTransform != VK_SURFACE_TRANSFORM_ROTATE_270_BIT_KHR) {
      needManualTransform = true;
    }
  } else {
    // For portrait, we expect ROTATE_90 or ROTATE_180
    if(swapChainSupport.capabilities.currentTransform != VK_SURFACE_TRANSFORM_ROTATE_90_BIT_KHR &&
       swapChainSupport.capabilities.currentTransform != VK_SURFACE_TRANSFORM_ROTATE_180_BIT_KHR) {
      needManualTransform = true;
    }
  }

  if(needManualTransform && (swapChainSupport.capabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)) {
    selectedTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    Tempest::Log::i("Android rotation fix: Using manual IDENTITY transform for ", isLandscape ? "landscape" : "portrait");
  }
#endif

  createInfo.preTransform = selectedTransform;

  // Log the final transform being used
  const char* transformName = "UNKNOWN";
  switch(selectedTransform) {
    case VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR:           transformName = "IDENTITY (0°)"; break;
    case VK_SURFACE_TRANSFORM_ROTATE_90_BIT_KHR:          transformName = "ROTATE_90 (90°)"; break;
    case VK_SURFACE_TRANSFORM_ROTATE_180_BIT_KHR:         transformName = "ROTATE_180 (180°)"; break;
    case VK_SURFACE_TRANSFORM_ROTATE_270_BIT_KHR:         transformName = "ROTATE_270 (270°)"; break;
    case VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_BIT_KHR:  transformName = "HORIZONTAL_MIRROR"; break;
    case VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_90_BIT_KHR: transformName = "HORIZONTAL_MIRROR_ROTATE_90"; break;
    case VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_180_BIT_KHR: transformName = "HORIZONTAL_MIRROR_ROTATE_180"; break;
    case VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_270_BIT_KHR: transformName = "HORIZONTAL_MIRROR_ROTATE_270"; break;
    case VK_SURFACE_TRANSFORM_INHERIT_BIT_KHR:             transformName = "INHERIT"; break;
  }
  Tempest::Log::i("Vulkan surface preTransform: ", transformName, " (", uint32_t(selectedTransform), ")");
  createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  createInfo.presentMode    = presentMode;
  createInfo.clipped        = VK_FALSE;

  if(vkCreateSwapchainKHR(device.device.impl, &createInfo, nullptr, &swapChain) != VK_SUCCESS)
    throw std::system_error(Tempest::GraphicsErrc::NoDevice);

  swapChainImageFormat = surfaceFormat.format;
  swapChainExtent      = extent;

  Tempest::Log::i("VSwapchain: swapChainExtent set to ", extent.width, "x", extent.height);

  createImageViews(device);

  sync.resize(views.size());

  aquireFence  = FenceList(device.device.impl, uint32_t(views.size()));
  presentFence = FenceList(device.device.impl, uint32_t(views.size()));
  presentSem   = SemaphoreList(device.device.impl, uint32_t(views.size()));
  aquireSem    = SemaphoreList(device.device.impl, uint32_t(views.size()));

  return implAcquireNextImage();
  }

void VSwapchain::createImageViews(VDevice &device) {
  uint32_t imgCount=0;
  vkGetSwapchainImagesKHR(device.device.impl, swapChain, &imgCount, nullptr);
  images.resize(imgCount);
  vkGetSwapchainImagesKHR(device.device.impl, swapChain, &imgCount, images.data());

  views.resize(images.size());

  for(size_t i=0; i<images.size(); i++) {
    VkImageViewCreateInfo createInfo = {};
    createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    createInfo.image = images[i];
    createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    createInfo.format = swapChainImageFormat;
    createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    createInfo.subresourceRange.baseMipLevel   = 0;
    createInfo.subresourceRange.levelCount     = 1;
    createInfo.subresourceRange.baseArrayLayer = 0;
    createInfo.subresourceRange.layerCount     = 1;

    if(vkCreateImageView(device.device.impl,&createInfo,nullptr,&views[i])!=VK_SUCCESS)
      throw std::system_error(Tempest::GraphicsErrc::NoDevice);
    }
  }

VkSurfaceFormatKHR VSwapchain::findSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats) {
  if(availableFormats.size()==1 && availableFormats[0].format==VK_FORMAT_UNDEFINED)
    return {VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};

  for(const auto& availableFormat : availableFormats) {
    if(availableFormat.format==VK_FORMAT_B8G8R8A8_UNORM &&
       availableFormat.colorSpace==VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
      return availableFormat;
    }

  return availableFormats[0];
  }

VkPresentModeKHR VSwapchain::findSwapPresentMode(const std::vector<VkPresentModeKHR> &availablePresentModes) {
  /** intel says mailbox is better option for games
    * https://software.intel.com/content/www/us/en/develop/articles/api-without-secrets-introduction-to-vulkan-part-2.html
    **/
  std::initializer_list<VkPresentModeKHR> modes={
    VK_PRESENT_MODE_FIFO_KHR,         // vsync; optimal
    VK_PRESENT_MODE_MAILBOX_KHR,      // vsync; wait until vsync
    VK_PRESENT_MODE_FIFO_RELAXED_KHR, // vsync; optimal, but tearing
    VK_PRESENT_MODE_IMMEDIATE_KHR,    // no vsync
    };

  for(auto mode:modes){
    for(const auto available:availablePresentModes)
      if(available==mode)
        return mode;
    }

  // no good modes
  return VK_PRESENT_MODE_FIFO_KHR;
  }

VkExtent2D VSwapchain::findSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities,
                                      uint32_t w,uint32_t h) {
  if(capabilities.currentExtent.width!=std::numeric_limits<uint32_t>::max()) {
    return capabilities.currentExtent;
    }

  VkExtent2D actualExtent = {
    static_cast<uint32_t>(w),
    static_cast<uint32_t>(h)
    };

  actualExtent.width  = std::clamp(actualExtent.width,  capabilities.minImageExtent.width,  capabilities.maxImageExtent.width );
  actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

  return actualExtent;
  }

uint32_t VSwapchain::findImageCount(const SwapChainSupport& support) const {
  const uint32_t maxImages = support.capabilities.maxImageCount==0 ? uint32_t(-1) : support.capabilities.maxImageCount;
  const uint32_t minImages = support.capabilities.minImageCount;

  /* NOTE: https://github.com/KhronosGroup/Vulkan-Docs/issues/1158#issuecomment-573874821
   * It's not clear how many images make a good fit
   */
  uint32_t imageCount = minImages + 1;

  imageCount = std::clamp(imageCount, minImages, maxImages);
  return imageCount;
  }

void VSwapchain::acquireNextImage() {
  VkResult code = implAcquireNextImage();

  if(code==VK_ERROR_OUT_OF_DATE_KHR) {
    Tempest::Log::i("VSwapchain::acquireNextImage: OUT_OF_DATE - rotation likely occurred");
    throw SwapchainSuboptimal();
  }
#if defined(__ANDROID__)
  // On Android, ignore SUBOPTIMAL to avoid constant swapchain recreation
  if(code!=VK_SUCCESS && code!=VK_SUBOPTIMAL_KHR)
    vkAssert(code);
#else
  if(code==VK_SUBOPTIMAL_KHR) {
    Tempest::Log::i("VSwapchain::acquireNextImage: SUBOPTIMAL - rotation change detected");
    throw SwapchainSuboptimal();
  }
  if(code!=VK_SUCCESS)
    vkAssert(code);
#endif
  }

uint32_t VSwapchain::currentBackBufferIndex() {
  return imgIndex;
  }

VkResult VSwapchain::implAcquireNextImage() {
  auto     dev  = device.device.impl;
  auto&    slot = sync[frameId];

  vkWaitForFences(dev, 1, &aquireFence[frameId], VK_TRUE, std::numeric_limits<uint64_t>::max());
  vkResetFences(dev, 1, &aquireFence[frameId]);

  // We need to wait for any command-buffer that may reference aquireSem[] in the past
  vkWaitForFences(dev, 1, &presentFence[frameId], VK_TRUE, std::numeric_limits<uint64_t>::max());
  vkResetFences(dev, 1, &presentFence[frameId]);

  uint32_t id   = uint32_t(-1);
  VkResult code = vkAcquireNextImageDBG(device.device.impl,
                                        swapChain,
                                        std::numeric_limits<uint64_t>::max(),
                                        aquireSem[frameId],
                                        aquireFence[frameId],
                                        &id);

  if(code==VK_ERROR_OUT_OF_DATE_KHR) {
    auto rc = vkxRevertFence(device.device.impl, &aquireFence[frameId]);
    if(rc!=VK_SUCCESS)
      std::terminate(); // unrecoverable
    return code;
    }

  if(code!=VK_SUCCESS && code!=VK_SUBOPTIMAL_KHR)
    vkAssert(code);

  imgIndex     = id;
  slot.state   = S_Pending;
  slot.imgId   = id;
  slot.acquire = aquireSem[frameId];

  return code;
  }

void VSwapchain::present() {
  auto& slot = sync[frameId];
  auto& pf   = presentFence[frameId];
  auto  ps   = presentSem  [imgIndex];
  if(slot.state==S_Pending) {
    // empty frame with no command buffer. TODO: test
    ps = aquireSem[frameId];
    }

  if(false && device.vkQueueSubmit2!=nullptr) {
    VkSemaphoreSubmitInfoKHR signal = {};
    signal.sType     = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO_KHR;
    signal.semaphore = ps;
    signal.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR;

    VkSubmitInfo2KHR submitInfo = {};
    submitInfo.sType                    = VK_STRUCTURE_TYPE_SUBMIT_INFO_2_KHR;
    submitInfo.signalSemaphoreInfoCount = 1;
    submitInfo.pSignalSemaphoreInfos    = &signal;

    device.graphicsQueue->submit(1, &submitInfo, pf, device.vkQueueSubmit2);
    } else {
    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores    = &ps;
    device.graphicsQueue->submit(1, &submitInfo, pf);
    }

  VkPresentInfoKHR presentInfo = {};
  presentInfo.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  presentInfo.waitSemaphoreCount = 1;
  presentInfo.pWaitSemaphores    = &ps;
  presentInfo.swapchainCount     = 1;
  presentInfo.pSwapchains        = &swapChain;
  presentInfo.pImageIndices      = &imgIndex;

  frameId      = (frameId + 1)%images.size();
  slot.state   = S_Idle;
  slot.imgId   = uint32_t(-1);
  slot.acquire = VK_NULL_HANDLE;

  auto tx = Application::tickCount();
  VkResult code = device.presentQueue->present(presentInfo);
  if(code==VK_ERROR_OUT_OF_DATE_KHR) {
    Tempest::Log::i("VSwapchain::present: OUT_OF_DATE - rotation detected during present");
    throw SwapchainSuboptimal();
  }
#if defined(__ANDROID__)
  // On Android, ignore SUBOPTIMAL to avoid constant swapchain recreation
  if(code!=VK_SUCCESS && code!=VK_SUBOPTIMAL_KHR)
    Detail::vkAssert(code);
#else
  if(code==VK_SUBOPTIMAL_KHR) {
    Tempest::Log::i("VSwapchain::present: SUBOPTIMAL - rotation change during present");
    throw SwapchainSuboptimal();
  }
  Detail::vkAssert(code);
#endif
  tx = Application::tickCount()-tx;
  if(tx > 2) {
    // std::chrono::system_clock::time_point p = std::chrono::system_clock::now();
    // time_t t = std::chrono::system_clock::to_time_t(p);
    // char str[26] = {};
    // strftime(str, sizeof(str), "%H:%M.%S", localtime(&t));
    // Log::i(str," : vkQueuePresentKHR[",imgIndex,"] = ", tx);
    }

  acquireNextImage();
  }

#endif

