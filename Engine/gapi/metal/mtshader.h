#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <Tempest/UniformsLayout>

#import <Metal/MTLDevice.h>

#include "nsptr.h"

namespace Tempest {
namespace Detail {

class MtShader : public AbstractGraphicsApi::Shader {
  public:
    MtShader(id dev, const void* source, size_t srcSize);
    using Binding = UniformsLayout::Binding;

    NsPtr library;
    NsPtr impl;

    std::vector<Decl::ComponentType> vdecl;
    std::vector<Binding>             lay;
  };

}
}
