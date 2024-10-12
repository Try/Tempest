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

IVec3 ComputePipeline::workGroupSize() const {
  if(impl.handler==nullptr)
    return IVec3();
  return impl.handler->workGroupSize();
  }

size_t ComputePipeline::sizeOfBuffer(size_t layoutBind, size_t arraylen) const {
  return ulay.sizeOfBuffer(layoutBind, arraylen);
  }
