#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <Tempest/Size>

#include "../utility/dptr.h"

namespace Tempest {

class HeadlessDevice;
class CommandBuffer;
class UniformsLayout;

class RenderPipeline final {
  public:
    RenderPipeline()=default;
    RenderPipeline(RenderPipeline&& f)=default;
    ~RenderPipeline();
    RenderPipeline& operator = (RenderPipeline&& other);

    bool isEmpty() const { return impl.handler==nullptr; }

  private:
    RenderPipeline(Tempest::HeadlessDevice& dev,Detail::DSharedPtr<AbstractGraphicsApi::Pipeline*>&& p);

    Tempest::HeadlessDevice*                           dev=nullptr;
    Detail::DSharedPtr<AbstractGraphicsApi::Pipeline*> impl;

  friend class Tempest::HeadlessDevice;
  friend class Tempest::CommandBuffer;

  template<class T>
  friend class Tempest::Detail::ResourcePtr;
  };

}
