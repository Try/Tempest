#pragma once

#include <Tempest/AbstractGraphicsApi>

#include "gapi/shaderreflection.h"

namespace Tempest {
namespace Detail {

class Shader : public AbstractGraphicsApi::Shader {
  protected:
    using Binding = ShaderReflection::Binding;

    Shader() = default;
    Shader(const void *source, size_t size);

    void fetchBindings(const uint32_t* source, const size_t size);

  public:
    const char* dbgShortName() const;

    struct Vert {
      std::vector<Decl::ComponentType> decl;

      bool has_baseVertex_baseInstance = false;
      };
    struct Comp {
      IVec3 wgSize;
      bool  has_NumworkGroups = false;
      };
    struct Dbg {
      std::string source;
      };

    ShaderReflection::Stage stage = ShaderReflection::Stage::Compute;
    std::vector<Binding>    lay;

    Vert                    vert;
    Comp                    comp;
    Dbg                     dbg;
  };

}
}
