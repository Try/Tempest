#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <Tempest/PipelineLayout>

#include "gapi/shader.h"

#include <Metal/Metal.hpp>
#include "nsptr.h"

namespace Tempest {
namespace Detail {

class MtDevice;

class MtShader : public Detail::Shader {
  public:
    MtShader(MtDevice& dev, const void* source, size_t srcSize);
    ~MtShader();

    enum Bindings : uint32_t {
      MSL_BUFFER_LENGTH      = 29,
      MSL_BUFFER_LENGTH_SIZE = 30,
      };

    NsPtr<MTL::Library>  library;
    NsPtr<MTL::Function> impl;
    bool                 bufferSizeBuffer = false;
  };

}
}
