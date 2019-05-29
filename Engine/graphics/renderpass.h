#pragma once

#include <Tempest/AbstractGraphicsApi>
#include "../utility/dptr.h"

namespace Tempest {

class Device;
class CommandBuffer;
class PrimaryCommandBuffer;

class RenderPass final {
  public:
    RenderPass()=default;
    RenderPass(RenderPass&& f)=default;
    ~RenderPass();
    RenderPass& operator = (RenderPass&& other)=default;

  private:
    RenderPass(Detail::DSharedPtr<AbstractGraphicsApi::Pass*>&& img);
    Detail::DSharedPtr<AbstractGraphicsApi::Pass*> impl;

  friend class Tempest::Device;
  friend class Tempest::CommandBuffer;
  friend class Tempest::PrimaryCommandBuffer;
  };

}
