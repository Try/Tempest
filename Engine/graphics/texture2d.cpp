#include "texture2d.h"

#include <Tempest/Device>
#include <algorithm>

using namespace Tempest;

Texture2d::Texture2d(Device &dev, AbstractGraphicsApi::Texture *impl)
  :dev(&dev),impl(impl) {
  }

Texture2d::~Texture2d(){
  if(dev)
    dev->destroy(*this);
  }

