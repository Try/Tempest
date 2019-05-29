#include "renderpipeline.h"

#include <Tempest/Device>

using namespace Tempest;

RenderPipeline::RenderPipeline(Device &dev, Detail::DSharedPtr<AbstractGraphicsApi::Pipeline *> &&p)
  :dev(&dev),impl(std::move(p)) {
  }

RenderPipeline::~RenderPipeline() {
  }

RenderPipeline &RenderPipeline::operator =(RenderPipeline&& other) {
  impl = std::move(other.impl);
  std::swap(dev,other.dev);
  return *this;
  }
