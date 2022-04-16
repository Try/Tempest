#pragma once

#include <Tempest/AbstractGraphicsApi>

#include "gapi/shaderreflection.h"

namespace Tempest {
namespace Detail {

class MtShader;

class MtPipelineLay : public AbstractGraphicsApi::PipelineLay {
  public:
    using Binding = ShaderReflection::Binding;

    static constexpr const ShaderReflection::Stage vertMask
      = ShaderReflection::Stage(ShaderReflection::Vertex|ShaderReflection::Control|ShaderReflection::Evaluate|ShaderReflection::Geometry);

    enum MTLStage : uint8_t {
      Vertex,
      Fragment,
      Compute
      };

    struct MTLBind {
      uint32_t bindVs      = uint32_t(-1);
      uint32_t bindVsSmp   = uint32_t(-1);
      uint32_t bindFs      = uint32_t(-1);
      uint32_t bindFsSmp   = uint32_t(-1);
      uint32_t bindCs      = uint32_t(-1);
      uint32_t bindCsSmp   = uint32_t(-1);
      uint32_t mslPushSize = 0;
      };

    MtPipelineLay(const std::vector<Binding> **sh, size_t cnt);

    size_t descriptorsCount() override;

    void adjustSsboBindings();

    std::vector<Binding>          lay;
    std::vector<MTLBind>          bind;

    ShaderReflection::PushBlock   pb;
    MTLBind                       bindPush;

    uint32_t                      vboIndex = 0;
    bool                          hasSSBO = false;
  };

}
}
