#include "vframebufferlayout.h"

using namespace Tempest::Detail;

VFramebufferLayout::VFramebufferLayout(VDevice& dev,VSwapchain &sw, const VkFormat *attach, uint8_t attCount)
  :rp(dev,sw,attach,attCount){
  }
