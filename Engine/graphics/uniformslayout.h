#pragma once

#include <Tempest/AbstractGraphicsApi>
#include "../utility/dptr.h"
#include "../utility/spinlock.h"

#include <cstdint>
#include <vector>
#include <memory>

namespace Tempest {

class Device;

class UniformsLayout final {
  public:
    UniformsLayout(UniformsLayout&& other)=default;
    UniformsLayout& operator = (UniformsLayout&& other)=default;

  private:
    UniformsLayout()=default;
    UniformsLayout(Detail::DSharedPtr<AbstractGraphicsApi::UniformsLay*>&& lay);
    Detail::DSharedPtr<AbstractGraphicsApi::UniformsLay*> impl;

  friend class Device;
  friend class RenderPipeline;
  friend class ComputePipeline;
  };

}
