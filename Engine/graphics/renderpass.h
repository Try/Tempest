#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <Tempest/Color>

#include "../utility/dptr.h"

namespace Tempest {

class Device;
class CommandBuffer;
class PrimaryCommandBuffer;

class Attachment final {
  public:
    Attachment()=default;
    Attachment(const FboMode& m):mode(m){}
    Attachment(const float& clr,const TextureFormat& frm):mode(FboMode::Clear),clear(clr),format(frm){}
    Attachment(const Color& clr,const TextureFormat& frm):mode(FboMode::Clear),clear(clr),format(frm){}

    FboMode       mode=FboMode::Preserve;
    Color         clear{};
    TextureFormat format=TextureFormat::Undefined;
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

  friend class Tempest::Device;
  friend class Tempest::CommandBuffer;
  friend class Tempest::PrimaryCommandBuffer;
  };

}
