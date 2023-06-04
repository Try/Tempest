#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <Tempest/PipelineLayout>

#include "gapi/shaderreflection.h"

#include <Metal/Metal.hpp>
#include "nsptr.h"

namespace Tempest {
namespace Detail {

class MtDevice;

class MtShader : public AbstractGraphicsApi::Shader {
  public:
    MtShader(MtDevice& dev, const void* source, size_t srcSize);
    ~MtShader();

    enum Bindings : uint32_t {
      MSL_BUFFER_LENGTH      = 29,
      MSL_BUFFER_LENGTH_SIZE = 30,
    };

    using Binding = ShaderReflection::Binding;

    NsPtr<MTL::Library>              library;
    NsPtr<MTL::Function>             impl;

    std::vector<Decl::ComponentType> vdecl;
    std::vector<Binding>             lay;
    ShaderReflection::Stage          stage = ShaderReflection::Stage::Compute;
    bool                             bufferSizeBuffer = false;

    struct {
      MTL::Winding                   winding   = MTL::WindingClockwise;
      MTL::TessellationPartitionMode partition = MTL::TessellationPartitionModePow2;
      } tese;

    struct {
      MTL::Size localSize = {};
      } comp;

  private:
    void fetchBindings(const uint32_t *source, size_t size);
  };

}
}
