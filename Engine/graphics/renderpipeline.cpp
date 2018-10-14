#include "renderpipeline.h"

#include <Tempest/Device>

using namespace Tempest;

RenderPipeline::RenderPipeline(Device &dev, AbstractGraphicsApi::Pipeline *impl)
  :dev(&dev),impl(impl) {
  }

RenderPipeline::~RenderPipeline() {
  if(dev)
    dev->destroy(*this);
  }
