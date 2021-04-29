#pragma once

#include <Tempest/AbstractGraphicsApi>
#include "../utility/dptr.h"
#include "../utility/spinlock.h"

#include <cstdint>
#include <vector>
#include <memory>

namespace Tempest {

class Device;

class PipelineLayout final {
  public:
    PipelineLayout(PipelineLayout&& other)=default;
    PipelineLayout& operator = (PipelineLayout&& other)=default;

  private:
    PipelineLayout()=default;
    PipelineLayout(Detail::DSharedPtr<AbstractGraphicsApi::PipelineLay*>&& lay);
    Detail::DSharedPtr<AbstractGraphicsApi::PipelineLay*> impl;

  friend class Device;
  friend class RenderPipeline;
  friend class ComputePipeline;
  };

}
