#include "computepipeline.h"

#include <Tempest/Device>

using namespace Tempest;

ComputePipeline::ComputePipeline(Detail::DSharedPtr<AbstractGraphicsApi::CompPipeline*>&& p)
  : impl(std::move(p)) {
  }

ComputePipeline& ComputePipeline::operator = (ComputePipeline&& other) {
  impl = std::move(other.impl);
  return *this;
  }

IVec3 ComputePipeline::workGroupSize() const {
  if(impl.handler==nullptr)
    return IVec3();
  return impl.handler->workGroupSize();
  }

size_t ComputePipeline::sizeofBuffer(size_t layoutBind, size_t arraylen) const {
  return impl.handler->sizeofBuffer(layoutBind, arraylen);
  }
