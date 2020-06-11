#include "renderpipeline.h"

#include <Tempest/Device>

using namespace Tempest;

RenderPipeline::RenderPipeline(Device &dev,
                               Detail::DSharedPtr<AbstractGraphicsApi::Pipeline *> &&p,
                               Detail::DSharedPtr<AbstractGraphicsApi::UniformsLay*>&& ulay)
  :dev(&dev), ulay(std::move(ulay)),impl(std::move(p)) {
  }

RenderPipeline::~RenderPipeline() {
  }

RenderPipeline &RenderPipeline::operator =(RenderPipeline&& other) {
  ulay = std::move(other.ulay);
  impl = std::move(other.impl);
  std::swap(dev,other.dev);
  return *this;
  }
