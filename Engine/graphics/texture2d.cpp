#include "texture2d.h"

#include <Tempest/Device>
#include <algorithm>

using namespace Tempest;

Texture2d::Texture2d(Device &dev, AbstractGraphicsApi::Texture *impl, uint32_t w, uint32_t h)
  :dev(&dev),impl(impl),texW(w),texH(h) {
  }

Texture2d::~Texture2d(){
  //if(dev)
  //  dev->destroy(*this);
  }

