#include "videobuffer.h"

#include <Tempest/Device>
#include <algorithm>

using namespace Tempest;

VideoBuffer::VideoBuffer(Device &dev, AbstractGraphicsApi::Buffer *impl)
  :dev(&dev),impl(impl) {
  }

VideoBuffer::~VideoBuffer(){
  if(dev)
    dev->destroy(*this);
  }

