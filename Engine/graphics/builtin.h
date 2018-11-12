#pragma once

#include <Tempest/RenderPipeline>
#include <Tempest/Shader>
#include <Tempest/UniformsLayout>

namespace Tempest {

class Device;
class RenderPass;

class Builtin {
  private:
    Builtin(Device& owner);

  public:
    const Tempest::RenderPipeline& brushTexture2d(RenderPass& p, uint32_t w, uint32_t h) const;
    const Tempest::UniformsLayout& brushTexture2dLayout() const { return ulay; }

  private:
    mutable Tempest::RenderPipeline brush;
    Device&                 owner;

    Tempest::Shader         vs,fs;
    Tempest::UniformsLayout ulay;

  friend class Device;
  };

}
