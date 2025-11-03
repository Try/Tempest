#include "renderpipeline.h"

#include <Tempest/Device>

using namespace Tempest;

RenderPipeline::RenderPipeline(Detail::DSharedPtr<AbstractGraphicsApi::Pipeline *> &&p)
  : impl(std::move(p)) {
  }

RenderPipeline &RenderPipeline::operator =(RenderPipeline&& other) {
  impl = std::move(other.impl);
  return *this;
  }

size_t RenderPipeline::sizeofBuffer(size_t layoutBind, size_t arraylen) const {
  return impl.handler->sizeofBuffer(layoutBind, arraylen);
  }
