#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <Tempest/Color>

#include "../utility/dptr.h"

namespace Tempest {

class HeadlessDevice;
class CommandBuffer;
class PrimaryCommandBuffer;

class Attachment final {
  public:
    Attachment()=default;
    Attachment(const FboMode& m):mode(m){}
    Attachment(const FboMode& m,const float& clr):mode(m | FboMode::Clear),clear(clr){}
    Attachment(const FboMode& m,const Color& clr):mode(m | FboMode::Clear),clear(clr){}

    FboMode       mode=FboMode::Preserve;
    Color         clear{};
  };

class RenderPass final {
  public:
    RenderPass()=default;
    RenderPass(RenderPass&& f)=default;
    ~RenderPass();
    RenderPass& operator = (RenderPass&& other)=default;

  private:
    RenderPass(Detail::DSharedPtr<AbstractGraphicsApi::Pass*>&& img);
    Detail::DSharedPtr<AbstractGraphicsApi::Pass*> impl;

  friend class Tempest::HeadlessDevice;
  friend class Tempest::CommandBuffer;
  friend class Tempest::PrimaryCommandBuffer;
  };

}
