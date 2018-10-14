#include "renderpass.h"

#include <Tempest/Device>

using namespace Tempest;

RenderPass::RenderPass(Device &dev, AbstractGraphicsApi::Pass *impl)
  :dev(&dev),impl(impl) {
  }

RenderPass::~RenderPass() {
  if(dev)
    dev->destroy(*this);
  }
