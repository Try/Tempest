#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <Tempest/PipelineLayout>

#include "gapi/shaderreflection.h"

#import <Metal/MTLDevice.h>

namespace Tempest {
namespace Detail {

class MtDevice;

class MtShader : public AbstractGraphicsApi::Shader {
  public:
    MtShader(MtDevice& dev, const void* source, size_t srcSize);
    ~MtShader();

    using Binding = ShaderReflection::Binding;

    id<MTLLibrary>  library;
    id<MTLFunction> impl;

    std::vector<Decl::ComponentType> vdecl;
    std::vector<Binding>             lay;
  };

}
}
