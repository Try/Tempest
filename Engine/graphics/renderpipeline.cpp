#include "renderpipeline.h"

#include <Tempest/Device>

using namespace Tempest;

RenderPipeline::RenderPipeline(Detail::DSharedPtr<AbstractGraphicsApi::Pipeline *> &&p,
                               Detail::DSharedPtr<AbstractGraphicsApi::PipelineLay*>&& ulay)
  : ulay(std::move(ulay)),impl(std::move(p)) {
  }

RenderPipeline &RenderPipeline::operator =(RenderPipeline&& other) {
  ulay = std::move(other.ulay);
  impl = std::move(other.impl);
  return *this;
  }

size_t RenderPipeline::sizeofBuffer(size_t layoutBind, size_t arraylen) const {
  return ulay.sizeofBuffer(layoutBind, arraylen);
  }
