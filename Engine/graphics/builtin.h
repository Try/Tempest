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
    struct Item {
      Tempest::RenderPipeline pen;
      Tempest::RenderPipeline brush;
      Tempest::UniformsLayout layout;
      };

    const Item& texture2d(RenderPass& p, uint32_t w, uint32_t h) const;
    const Item& empty    (RenderPass& p, uint32_t w, uint32_t h) const;

    void  reset() const;

  private:
    mutable Item            brushT2;
    mutable Item            brushE;

    Device&                 owner;
    Tempest::Shader         vsT2,fsT2,vsE,fsE;

  friend class Device;
  };

}
