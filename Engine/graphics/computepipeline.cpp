#include "computepipeline.h"

#include <Tempest/Device>

using namespace Tempest;

ComputePipeline::ComputePipeline(Detail::DSharedPtr<AbstractGraphicsApi::CompPipeline*>&& p,
                                 Detail::DSharedPtr<AbstractGraphicsApi::PipelineLay*>&&  ulay)
  : ulay(std::move(ulay)),impl(std::move(p)) {
  }

ComputePipeline& ComputePipeline::operator = (ComputePipeline&& other) {
  ulay = std::move(other.ulay);
  impl = std::move(other.impl);
  return *this;
  }
