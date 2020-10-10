#pragma once

#include <Tempest/RenderPipeline>
#include <Tempest/ComputePipeline>
#include <Tempest/RenderState>
#include <Tempest/Shader>
#include <Tempest/UniformsLayout>

namespace Tempest {

class Device;
class RenderPass;

class Builtin final {
  private:
    Builtin(Device& owner);

  public:
    struct Item {
      Tempest::RenderPipeline pen;
      Tempest::RenderPipeline brush;

      Tempest::RenderPipeline penB;
      Tempest::RenderPipeline brushB;

      Tempest::RenderPipeline penA;
      Tempest::RenderPipeline brushA;
      };

    const Item& texture2d() const;
    const Item& empty    () const;

    const Tempest::ComputePipeline& blit() const { return blitPso; }

  private:
    mutable Item            brushT2;
    mutable Item            brushE;

    RenderState             stNormal, stBlend, stAlpha;
    Device&                 owner;
    Tempest::Shader         vsT2,fsT2,vsE,fsE;

    Tempest::ComputePipeline blitPso;

  friend class Device;
  };

}
