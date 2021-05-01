#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <Tempest/PipelineLayout>

#include "gapi/shaderreflection.h"

#import <Metal/MTLDevice.h>

namespace Tempest {
namespace Detail {

class MtShader : public AbstractGraphicsApi::Shader {
  public:
    MtShader(id dev, const void* source, size_t srcSize);
    using Binding = ShaderReflection::Binding;

    id<MTLLibrary>  library;
    id<MTLFunction> impl;

    std::vector<Decl::ComponentType> vdecl;
    std::vector<Binding>             lay;
  };

}
}
