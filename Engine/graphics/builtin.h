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
    Builtin(Device& device);

  public:
    struct Item {
      Tempest::RenderPipeline pen;
      Tempest::RenderPipeline brush;

      Tempest::RenderPipeline penB;
      Tempest::RenderPipeline brushB;

      Tempest::RenderPipeline penA;
      Tempest::RenderPipeline brushA;
      };

    const Item& texture2d() const { return brushT2; }
    const Item& empty    () const { return brushE;  }

  private:
    Item            mkShaderSet(bool textures);

    Device&         device;
    Item            brushT2;
    Item            brushE;

  friend class Device;
  };

}
