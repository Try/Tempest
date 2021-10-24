#pragma once

#include <Tempest/RenderPipeline>
#include <Tempest/ComputePipeline>
#include <Tempest/RenderState>
#include <Tempest/Shader>
#include <Tempest/PipelineLayout>

namespace Tempest {

class Device;

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

  private:
    mutable Item            brushT2;
    mutable Item            brushE;

    RenderState             stNormal, stBlend, stAlpha;
    Device&                 owner;
    Tempest::Shader         vsT2,fsT2,vsE,fsE;

  friend class Device;
  };

}
