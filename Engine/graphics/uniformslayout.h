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
    enum Class : uint8_t {
      Ubo    =0,
      Texture=1,
      SsboR  =2,
      SsboRW =3,
      ImgR   =4,
      ImgRW  =5,
      Push   =6,
      };
    enum Stage : uint8_t {
      Vertex  =1<<0,
      Fragment=1<<1,
      Compute =1<<2,
      };
    struct Binding {
      uint32_t layout=0;
      Class    cls   =Ubo;
      Stage    stage =Fragment;
      size_t   size  =0;
      };
    struct PushBlock {
      Stage    stage = Fragment;
      size_t   size  = 0;
      };

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
