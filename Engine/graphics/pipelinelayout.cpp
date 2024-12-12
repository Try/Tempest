#include "pipelinelayout.h"

using namespace Tempest;

PipelineLayout::PipelineLayout(Detail::DSharedPtr<AbstractGraphicsApi::PipelineLay*>&& impl)
  :impl(std::move(impl)) {
  }

size_t PipelineLayout::sizeofBuffer(size_t layoutBind, size_t arraylen) const {
  return impl.handler->sizeofBuffer(layoutBind, arraylen);
  }
