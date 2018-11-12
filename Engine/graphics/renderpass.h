#pragma once

#include <Tempest/AbstractGraphicsApi>
#include "../utility/dptr.h"

namespace Tempest {

class Device;
class CommandBuffer;

class RenderPass {
  public:
    enum FboMode {
      Preserve,
      Discard
      };
    RenderPass()=default;
    RenderPass(RenderPass&& f)=default;
    ~RenderPass();
    RenderPass& operator = (RenderPass&& other)=default;

  private:
    RenderPass(Tempest::Device& dev,AbstractGraphicsApi::Pass* img);

    Tempest::Device*                         dev=nullptr;
    Detail::DPtr<AbstractGraphicsApi::Pass*> impl;

  friend class Tempest::Device;
  friend class Tempest::CommandBuffer;
  };

}
