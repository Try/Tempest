#pragma once

#include <Tempest/RenderPipeline>
#include <Tempest/RenderState>
#include <Tempest/Shader>
#include <Tempest/UniformsLayout>

namespace Tempest {

class HeadlessDevice;
class RenderPass;

class Builtin {
  private:
    Builtin(HeadlessDevice& owner);

  public:
    struct Item {
      Tempest::RenderPipeline pen;
      Tempest::RenderPipeline brush;

      Tempest::RenderPipeline penB;
      Tempest::RenderPipeline brushB;

      Tempest::RenderPipeline penA;
      Tempest::RenderPipeline brushA;

      Tempest::UniformsLayout layout;
      };

    const Item& texture2d() const;
    const Item& empty    () const;

    void  reset() const;

  private:
    mutable Item            brushT2;
    mutable Item            brushE;

    RenderState             stNormal, stBlend, stAlpha;
    HeadlessDevice&         owner;
    Tempest::Shader         vsT2,fsT2,vsE,fsE;

  friend class HeadlessDevice;
  };

}
