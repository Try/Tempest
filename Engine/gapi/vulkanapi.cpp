#if defined(TEMPEST_BUILD_VULKAN)

#include "vulkanapi.h"

#include "vulkan/vdevice.h"
#include "vulkan/vswapchain.h"
#include "vulkan/vpipeline.h"
#include "vulkan/vbuffer.h"
#include "vulkan/vshader.h"
#include "vulkan/vfence.h"
#include "vulkan/vcommandbuffer.h"
#include "vulkan/vdescriptorarray.h"
#include "vulkan/vtexture.h"
#include "vulkan/vaccelerationstructure.h"

#include <Tempest/Pixmap>
#include <Tempest/Log>
#include <Tempest/Application>

#include <libspirv/libspirv.h>

using namespace Tempest;
using namespace Tempest::Detail;

#define VK_KHR_WIN32_SURFACE_EXTENSION_NAME   "VK_KHR_win32_surface"
#define VK_KHR_XLIB_SURFACE_EXTENSION_NAME    "VK_KHR_xlib_surface"
#define VK_KHR_ANDROID_SURFACE_EXTENSION_NAME "VK_KHR_android_surface"

#if defined(__WINDOWS__)
#define SURFACE_EXTENSION_NAME VK_KHR_WIN32_SURFACE_EXTENSION_NAME
#elif defined(__ANDROID__)
#define SURFACE_EXTENSION_NAME VK_KHR_ANDROID_SURFACE_EXTENSION_NAME
#elif defined(__UNIX__)
#define SURFACE_EXTENSION_NAME VK_KHR_XLIB_SURFACE_EXTENSION_NAME
#endif

static const std::initializer_list<const char*> validationLayersKHR = {
  "VK_LAYER_KHRONOS_validation"
  };

static const std::initializer_list<const char*> validationLayersLunarg = {
  "VK_LAYER_LUNARG_core_validation"
  };

static bool layerSupport(const std::vector<VkLayerProperties>& list, const std::initializer_list<const char*> dest) {
  for(auto& i:dest) {
    bool found=false;
    for(auto& r:list)
      if(std::strcmp(r.layerName,i)==0) {
        found = true;
        break;
        }
    if(!found)
      return false;
    }
  return true;
  }

static bool extensionSupport(const std::vector<VkExtensionProperties>& list, const char* name) {
  for(auto& r:list)
    if(std::strcmp(name,r.extensionName)==0)
      return true;
  return false;
  }

static const std::initializer_list<const char*>& checkValidationLayerSupport() {
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

static std::vector<VkExtensionProperties> instExtensionsList() {
  uint32_t extensionCount;
  vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);

  std::vector<VkExtensionProperties> ext(extensionCount);
  vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, ext.data());

  return ext;
  }


struct Tempest::VulkanApi::Impl {
  Impl(bool validation)
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
    if(extensionSupport(ext, VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME)) {
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
        Log::e("VulkanApi: unable to setup validation callback");
      }
    }

  ~Impl() {
    if(callback!=VK_NULL_HANDLE && vkDestroyDebugReportCallbackEXT!=nullptr)
      vkDestroyDebugReportCallbackEXT(instance, callback, nullptr);
    }

  bool checkDeviceExtensionSupport(VkPhysicalDevice device) const {
    auto ext = VDevice::extensionsList(device);
    for(auto& i:VDevice::requiredExtensions) {
      if(!extensionSupport(ext,i))
        return false;
      }
    return true;
    }

  bool isDeviceSuitable(VkPhysicalDevice device, const VDevice::VkProps props) const {
    bool extensionsSupported = checkDeviceExtensionSupport(device);
    return extensionsSupported && props.graphicsFamily!=uint32_t(-1);
    }

  static VkBool32 debugReportCallback(VkDebugReportFlagsEXT      flags,
                                      VkDebugReportObjectTypeEXT objectType,
                                      uint64_t                   object,
                                      size_t                     location,
                                      int32_t                    messageCode,
                                      const char                *pLayerPrefix,
                                      const char                *pMessage,
                                      void                      *pUserData);

  VkInstance                          instance   = VK_NULL_HANDLE;
  bool                                validation = false;
  bool                                hasDeviceFeatures2 = false;

  VkDebugReportCallbackEXT            callback   = VK_NULL_HANDLE;
  PFN_vkDestroyDebugReportCallbackEXT vkDestroyDebugReportCallbackEXT = nullptr;
  };


VulkanApi::VulkanApi(ApiFlags f) {
  impl.reset(new Impl(ApiFlags::Validation==(f&ApiFlags::Validation)));
  }

VulkanApi::~VulkanApi(){
  }

std::vector<AbstractGraphicsApi::Props> VulkanApi::devices() const {
  std::vector<Tempest::AbstractGraphicsApi::Props> devList;
  uint32_t deviceCount = 0;
  vkEnumeratePhysicalDevices(impl->instance, &deviceCount, nullptr);

  if(deviceCount==0)
    return devList;

  std::vector<VkPhysicalDevice> devices(deviceCount);
  vkEnumeratePhysicalDevices(impl->instance, &deviceCount, devices.data());

  devList.reserve(devices.size());
  for(auto device : devices) {
    VDevice::VkProps props = {};
    VDevice::deviceProps(impl->instance, impl->hasDeviceFeatures2, device, props);
    VDevice::deviceQueueProps(device, props);
    if(!impl->isDeviceSuitable(device, props))
      continue;
    devList.push_back(static_cast<Tempest::AbstractGraphicsApi::Props&>(props));
    }
  return devList;
  }

AbstractGraphicsApi::Device* VulkanApi::createDevice(std::string_view gpuName) {
  uint32_t deviceCount = 0;
  vkEnumeratePhysicalDevices(impl->instance, &deviceCount, nullptr);

  if(deviceCount==0)
    throw std::system_error(Tempest::GraphicsErrc::NoDevice);

  std::vector<VkPhysicalDevice> devices(deviceCount);
  vkEnumeratePhysicalDevices(impl->instance, &deviceCount, devices.data());

  for(const auto& device:devices) {
    VDevice::VkProps props = {};
    VDevice::deviceProps(impl->instance, impl->hasDeviceFeatures2, device, props);
    if(!gpuName.empty() && gpuName!=props.name)
      continue;
    VDevice::deviceQueueProps(device, props);
    if(!impl->isDeviceSuitable(device, props))
      continue;
    return new VDevice(impl->instance, impl->hasDeviceFeatures2, device);
    }

  throw std::system_error(Tempest::GraphicsErrc::NoDevice);
  }

AbstractGraphicsApi::Swapchain *VulkanApi::createSwapchain(SystemApi::Window *w, AbstractGraphicsApi::Device *d) {
  Detail::VDevice* dx   = reinterpret_cast<Detail::VDevice*>(d);
  return new Detail::VSwapchain(*dx, w);
  }

AbstractGraphicsApi::PPipeline VulkanApi::createPipeline(AbstractGraphicsApi::Device *d, const RenderState &st, Topology tp,
                                                         const Shader*const* sh, size_t cnt) {
  auto* dx = reinterpret_cast<Detail::VDevice*>(d);
  const Detail::VShader* shader[5] = {};
  for(size_t i=0; i<cnt; ++i)
    shader[i] = reinterpret_cast<const Detail::VShader*>(sh[i]);
  return PPipeline(new Detail::VPipeline(*dx,tp,st,shader,cnt));
  }

AbstractGraphicsApi::PCompPipeline VulkanApi::createComputePipeline(AbstractGraphicsApi::Device* d, AbstractGraphicsApi::Shader* shader) {
  auto* dx = reinterpret_cast<Detail::VDevice*>(d);
  return PCompPipeline(new Detail::VCompPipeline(*dx,*reinterpret_cast<Detail::VShader*>(shader)));
  }

AbstractGraphicsApi::PShader VulkanApi::createShader(AbstractGraphicsApi::Device *d, const void* source, size_t src_size) {
  Detail::VDevice* dx=reinterpret_cast<Detail::VDevice*>(d);
  return PShader(new Detail::VShader(*dx,source,src_size));
  }

AbstractGraphicsApi::PBuffer VulkanApi::createBuffer(AbstractGraphicsApi::Device *d, const void *mem, size_t size,
                                                     MemUsage usage, BufferHeap flg) {
  Detail::VDevice& dx = *reinterpret_cast<Detail::VDevice*>(d);

  if(flg==BufferHeap::Upload) {
    VBuffer buf = dx.allocator.alloc(mem, size, usage, BufferHeap::Upload);
    return PBuffer(new VBuffer(std::move(buf)));
    }

  if(flg==BufferHeap::Readback) {
    VBuffer buf = dx.allocator.alloc(mem, size, usage, BufferHeap::Readback);
    return PBuffer(new VBuffer(std::move(buf)));
    }

  VBuffer buf = dx.allocator.alloc(nullptr, size, usage, BufferHeap::Device);
  if(mem==nullptr && (usage&MemUsage::Initialized)==MemUsage::Initialized) {
    DSharedPtr<VBuffer*> pbuf(new VBuffer(std::move(buf)));
    pbuf.handler->fill(0x0,0,size);
    return PBuffer(pbuf.handler);
    }
  if(mem==nullptr) {
    return PBuffer(new VBuffer(std::move(buf)));
    }

  DSharedPtr<Buffer*> pbuf(new VBuffer(std::move(buf)));
  pbuf.handler->update(mem,0,size);
  return PBuffer(pbuf.handler);
  }

AbstractGraphicsApi::PTexture VulkanApi::createTexture(AbstractGraphicsApi::Device *d, const Pixmap &p, TextureFormat frm, uint32_t mipCnt) {
  VDevice&       dx     = *reinterpret_cast<VDevice*>(d);

  const uint32_t size   = uint32_t(p.dataSize());
  VkFormat       format = Detail::nativeFormat(frm);

  VBuffer        stage  = dx.allocator.alloc(p.data(),size,MemUsage::Transfer,BufferHeap::Upload);
  VTexture       tex    = dx.allocator.alloc(p,mipCnt,format);

  DSharedPtr<Buffer*>  pstage(new VBuffer (std::move(stage)));
  DSharedPtr<Texture*> ptex  (new VTexture(std::move(tex)));

  auto cmd = dx.dataMgr().get();
  cmd->begin(SyncHint::NoPendingReads);
  cmd->hold(pstage);
  cmd->hold(ptex);
  cmd->discard(*ptex.handler);

  if(isCompressedFormat(frm)) {
    size_t blockSize  = Pixmap::blockSizeForFormat(frm);
    size_t bufferSize = 0;

    uint32_t w = uint32_t(p.w()), h = uint32_t(p.h());
    for(uint32_t i=0; i<mipCnt; i++){
      cmd->copy(*ptex.handler,w,h,i,*pstage.handler,bufferSize);

      Size bsz   = Pixmap::blockCount(frm,w,h);
      bufferSize += bsz.w*bsz.h*blockSize;
      w = std::max<uint32_t>(1,w/2);
      h = std::max<uint32_t>(1,h/2);
      }
    } else {
    cmd->copy(*ptex.handler,p.w(),p.h(),0,*pstage.handler,0);
    if(mipCnt>1)
      cmd->generateMipmap(*ptex.handler, p.w(), p.h(), mipCnt);
    }
  cmd->end();
  dx.dataMgr().submit(std::move(cmd));

  reinterpret_cast<VTexture*>(ptex.handler)->nonUniqId = NonUniqResId::I_None;

  return PTexture(ptex.handler);
  }

AbstractGraphicsApi::PTexture VulkanApi::createTexture(AbstractGraphicsApi::Device *d,
                                                       const uint32_t w, const uint32_t h, uint32_t mipCnt,
                                                       TextureFormat frm) {
  Detail::VDevice* dx = reinterpret_cast<Detail::VDevice*>(d);
  
  Detail::VTexture buf=dx->allocator.alloc(w,h,0,mipCnt,frm,false);
  Detail::DSharedPtr<Detail::VTexture*> pbuf(new Detail::VTextureWithFbo(std::move(buf)));

  return PTexture(pbuf.handler);
  }

AbstractGraphicsApi::PTexture VulkanApi::createStorage(Device* d,
                                                       const uint32_t w, const uint32_t h, uint32_t mipCnt,
                                                       TextureFormat frm) {
  Detail::VDevice& dx  = *reinterpret_cast<Detail::VDevice*>(d);

  Detail::VTexture buf = dx.allocator.alloc(w,h,0,mipCnt,frm,true);
  Detail::DSharedPtr<Texture*> pbuf(new Detail::VTexture(std::move(buf)));

  auto cmd = dx.dataMgr().get();
  cmd->begin(SyncHint::NoPendingReads);
  cmd->discard(*pbuf.handler);
  cmd->fill(*pbuf.handler, 0);
  cmd->hold(pbuf);
  cmd->end();
  dx.dataMgr().submit(std::move(cmd));

  return PTexture(pbuf.handler);
  }

AbstractGraphicsApi::PTexture VulkanApi::createStorage(Device* d,
                                                       const uint32_t w, const uint32_t h, const uint32_t depth, uint32_t mipCnt,
                                                       TextureFormat frm) {
  Detail::VDevice& dx = *reinterpret_cast<Detail::VDevice*>(d);

  Detail::VTexture buf=dx.allocator.alloc(w,h,depth,mipCnt,frm,true);
  Detail::DSharedPtr<Texture*> pbuf(new Detail::VTexture(std::move(buf)));

  auto cmd = dx.dataMgr().get();
  cmd->begin();
  cmd->discard(*pbuf.handler);
  cmd->fill(*pbuf.handler, 0);
  cmd->hold(pbuf);
  cmd->end();
  dx.dataMgr().submit(std::move(cmd));

  return PTexture(pbuf.handler);
  }

AbstractGraphicsApi::AccelerationStructure* VulkanApi::createBottomAccelerationStruct(Device* d, const RtGeometry* geom, size_t size) {
  auto& dx = *reinterpret_cast<VDevice*>(d);
  return new VAccelerationStructure(dx, geom, size);
  }

AbstractGraphicsApi::AccelerationStructure* VulkanApi::createTopAccelerationStruct(Device* d, const RtInstance* inst, AccelerationStructure*const* as, size_t size) {
  auto& dx = *reinterpret_cast<VDevice*>(d);
  return new VTopAccelerationStructure(dx, inst, as, size);
  }

void VulkanApi::readPixels(AbstractGraphicsApi::Device *d, Pixmap& out, const PTexture t,
                           TextureFormat frm, const uint32_t w, const uint32_t h, uint32_t mip, bool storageImg) {
  auto&           dx     = *reinterpret_cast<VDevice*>(d);
  auto&           tx     = *reinterpret_cast<VTexture*>(t.handler);

  size_t          bpb    = Pixmap::blockSizeForFormat(frm);
  Size            bsz    = Pixmap::blockCount(frm,w,h);

  const size_t    size   = bsz.w*bsz.h*bpb;
  Detail::VBuffer stage  = dx.allocator.alloc(nullptr, size, MemUsage::Transfer, BufferHeap::Readback);

  auto cmd = dx.dataMgr().get();
  cmd->begin(SyncHint::NoPendingReads);
  cmd->copy(stage,0, tx,w,h,mip);
  cmd->end();

  dx.dataMgr().submitAndWait(std::move(cmd));

  out = Pixmap(w,h,frm);
  stage.read(out.data(),0,size);
  }

void VulkanApi::readBytes(AbstractGraphicsApi::Device*, AbstractGraphicsApi::Buffer* buf, void* out, size_t size) {
  Detail::VBuffer&  bx = *reinterpret_cast<Detail::VBuffer*>(buf);
  bx.read(out,0,size);
  }

AbstractGraphicsApi::DescArray *VulkanApi::createDescriptors(Device *d, Texture **tex, size_t cnt, uint32_t mipLevel, const Sampler &smp) {
  auto& dx = *reinterpret_cast<Detail::VDevice*>(d);
  return new Detail::VDescriptorArray(dx, tex, cnt, mipLevel, smp);
  }

AbstractGraphicsApi::DescArray *VulkanApi::createDescriptors(Device* d, Texture** tex, size_t cnt, uint32_t mipLevel) {
  auto& dx = *reinterpret_cast<Detail::VDevice*>(d);
  return new Detail::VDescriptorArray(dx, tex, cnt, mipLevel);
  }

AbstractGraphicsApi::DescArray *VulkanApi::createDescriptors(Device *d, Buffer **buf, size_t cnt) {
  auto& dx = *reinterpret_cast<Detail::VDevice*>(d);
  return new Detail::VDescriptorArray(dx, buf, cnt);
  }

AbstractGraphicsApi::CommandBuffer* VulkanApi::createCommandBuffer(AbstractGraphicsApi::Device* d) {
  Detail::VDevice* dx=reinterpret_cast<Detail::VDevice*>(d);
  return new Detail::VCommandBuffer(*dx);
  }

void VulkanApi::present(Device*, Swapchain *sw) {
  Detail::VSwapchain* sx=reinterpret_cast<Detail::VSwapchain*>(sw);
  sx->present();
  }

std::shared_ptr<AbstractGraphicsApi::Fence> VulkanApi::submit(Device *d, CommandBuffer* cmd) {
  Detail::VDevice&        dx = *reinterpret_cast<Detail::VDevice*>(d);
  Detail::VCommandBuffer& cx = *reinterpret_cast<Detail::VCommandBuffer*>(cmd);
  auto fn = dx.submit(cx);
  return fn;
  }

void VulkanApi::getCaps(Device *d, Props& props) {
  Detail::VDevice* dx=reinterpret_cast<Detail::VDevice*>(d);
  props=dx->props;
  }

VkBool32 VulkanApi::Impl::debugReportCallback(VkDebugReportFlagsEXT      flags,
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
  if(location==3820618282) {
    // The Validation Layers hit a timeout waiting for fence state to update
    // Very anoying during game debuging!
    return VK_FALSE;
    }

  Log::e("VulkanApi: ", pMessage," object=",object,", type=",objectType," th:",std::this_thread::get_id());
  return VK_FALSE;
  }

#endif
