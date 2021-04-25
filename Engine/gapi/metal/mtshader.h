#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <Tempest/UniformsLayout>

#include "gapi/shaderreflection.h"

#import <Metal/MTLDevice.h>

#include "nsptr.h"

namespace Tempest {
namespace Detail {

class MtShader : public AbstractGraphicsApi::Shader {
  public:
    MtShader(id dev, const void* source, size_t srcSize);
    using Binding = ShaderReflection::Binding;

    NsPtr library;
    NsPtr impl;

    std::vector<Decl::ComponentType> vdecl;
    std::vector<Binding>             lay;
  };

}
}
