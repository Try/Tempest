#include "pipelinelayout.h"

using namespace Tempest;

PipelineLayout::PipelineLayout(Detail::DSharedPtr<AbstractGraphicsApi::PipelineLay*>&& impl)
  :impl(std::move(impl)) {
  }
