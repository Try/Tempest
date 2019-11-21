#include "vframebufferlayout.h"

#include "vdevice.h"

using namespace Tempest::Detail;

VFramebufferLayout::VFramebufferLayout(VDevice& dev,VSwapchain &sw, const VkFormat *attach, uint8_t attCount)
  :attCount(attCount), swapchain(&sw), device(dev.device) {
  impl = VRenderPass::createLayoutInstance(dev.device,sw,attach,attCount);
  frm.reset(new VkFormat[attCount]);
  for(size_t i=0;i<attCount;++i)
    frm[i] = attach[i];
  }

VFramebufferLayout::VFramebufferLayout(VFramebufferLayout &&other)
  :impl(other.impl), frm(std::move(other.frm)), attCount(other.attCount), swapchain(other.swapchain), device(other.device) {
  other.impl = VK_NULL_HANDLE;
  }

VFramebufferLayout::~VFramebufferLayout() {
  if(device==nullptr)
    return;
  vkDeviceWaitIdle(device);
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
  for(size_t i=0;i<attCount;++i)
    if(frm[i]!=other.frm[i])
      return false;
  return true;
  }

bool VFramebufferLayout::equals(const Tempest::AbstractGraphicsApi::FboLayout &other) {
  return isCompatible(reinterpret_cast<const VFramebufferLayout&>(other));
  }
