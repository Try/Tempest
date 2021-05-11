#if defined(TEMPEST_BUILD_VULKAN)

#include "vframebufferlayout.h"

#include "vdevice.h"
#include "vswapchain.h"

using namespace Tempest::Detail;

VFramebufferLayout::VFramebufferLayout(VDevice& dev, VSwapchain** sw, const VkFormat *attach, uint8_t attCount)
  :attCount(attCount), device(dev.device.impl) {
  frm.reset(new VkFormat[attCount]);
  swapchain.reset(new VSwapchain*[attCount]);
  for(size_t i=0;i<attCount;++i) {
    frm[i]       = attach[i];
    swapchain[i] = sw[i];
    if(!Detail::nativeIsDepthFormat(frm[i]))
      colorCount++;
    }
  impl = VRenderPass::createLayoutInstance(dev.device.impl,sw,attach,attCount);
  }

VFramebufferLayout::VFramebufferLayout(VFramebufferLayout &&other)
  :impl(other.impl), frm(std::move(other.frm)), swapchain(std::move(other.swapchain)), attCount(other.attCount), device(other.device) {
  other.impl = VK_NULL_HANDLE;
  }

VFramebufferLayout::~VFramebufferLayout() {
  if(device==nullptr)
    return;
  if(impl!=VK_NULL_HANDLE)
    vkDestroyRenderPass(device,impl,nullptr);
  }

void VFramebufferLayout::operator=(VFramebufferLayout &&other) {
  std::swap(impl,other.impl);
  std::swap(frm,other.frm);
  std::swap(attCount,other.attCount);
  std::swap(swapchain,other.swapchain);
  std::swap(device,other.device);
  }

bool VFramebufferLayout::isCompatible(const VFramebufferLayout &other) const {
  if(this==&other)
    return true;
  if(attCount!=other.attCount)
    return false;
  for(size_t i=0;i<attCount;++i) {
    if(frm[i]!=other.frm[i])
      return false;
    if(frm[i]==VK_FORMAT_UNDEFINED &&
       swapchain[i]->format()!=other.swapchain[i]->format())
      return false;
    }
  return true;
  }

bool VFramebufferLayout::equals(const Tempest::AbstractGraphicsApi::FboLayout &other) const {
  return isCompatible(reinterpret_cast<const VFramebufferLayout&>(other));
  }

#endif
