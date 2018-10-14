#pragma once

#include <Tempest/AbstractGraphicsApi>
#include "../utility/dptr.h"

namespace Tempest {

class Device;
class CommandBuffer;

class RenderPipeline {
  public:
    RenderPipeline()=default;
    RenderPipeline(RenderPipeline&& f)=default;
    ~RenderPipeline();
    RenderPipeline& operator = (RenderPipeline&& other)=default;

  private:
    RenderPipeline(Tempest::Device& dev,AbstractGraphicsApi::Pipeline* img);

    Tempest::Device*                             dev=nullptr;
    Detail::DPtr<AbstractGraphicsApi::Pipeline*> impl;

  friend class Tempest::Device;
  friend class Tempest::CommandBuffer;
  };

}
