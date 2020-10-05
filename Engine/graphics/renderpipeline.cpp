#include "renderpipeline.h"

#include <Tempest/Device>

using namespace Tempest;

RenderPipeline::RenderPipeline(Detail::DSharedPtr<AbstractGraphicsApi::Pipeline *> &&p,
                               Detail::DSharedPtr<AbstractGraphicsApi::UniformsLay*>&& ulay)
  : ulay(std::move(ulay)),impl(std::move(p)) {
  }

RenderPipeline &RenderPipeline::operator =(RenderPipeline&& other) {
  ulay = std::move(other.ulay);
  impl = std::move(other.impl);
  return *this;
  }
