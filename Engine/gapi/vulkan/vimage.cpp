#include "vimage.h"

#include "vdevice.h"

using namespace Tempest::Detail;

VImage::VImage(VDevice &dev, VkImage impl)
  :impl(impl),device(dev.device){
  }

VImage::VImage(VImage &&other) {
  std::swap(device,other.device);
  std::swap(impl,other.impl);
  }

VImage::~VImage() {
  if(device==nullptr)
    return;
  vkDeviceWaitIdle(device);
  //vkDestroyImage(device,impl,nullptr);
  }

