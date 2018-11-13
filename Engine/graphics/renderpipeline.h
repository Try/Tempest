#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <Tempest/Size>

#include "../utility/dptr.h"

namespace Tempest {

class Device;
class CommandBuffer;
class UniformsLayout;

class RenderPipeline final {
  public:
    RenderPipeline()=default;
    RenderPipeline(RenderPipeline&& f)=default;
    ~RenderPipeline();
    RenderPipeline& operator = (RenderPipeline&& other);

    uint32_t w() const;
    uint32_t h() const;

    Size size()    const { return Size(int(vpW),int(vpH)); }
    bool isEmpty() const { return impl.handler==nullptr; }

  private:
    RenderPipeline(Tempest::Device& dev,AbstractGraphicsApi::Pipeline* p,uint32_t w,uint32_t h);

    Tempest::Device*                                   dev=nullptr;
    Detail::DSharedPtr<AbstractGraphicsApi::Pipeline*> impl;

    uint32_t vpW=0,vpH=0;

  friend class Tempest::Device;
  friend class Tempest::CommandBuffer;

  template<class T>
  friend class Tempest::Detail::ResourcePtr;
  };

}
