#pragma once

#include <Tempest/AbstractGraphicsApi>

#include "gapi/shaderreflection.h"

namespace Tempest {
namespace Detail {

class MtShader;

class MtPipelineLay : public AbstractGraphicsApi::PipelineLay {
  public:
    using Binding    = ShaderReflection::Binding;
    using PushBlock  = ShaderReflection::PushBlock;
    using LayoutDesc = ShaderReflection::LayoutDesc;
    using SyncDesc   = ShaderReflection::SyncDesc;

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
      uint32_t bindTs      = uint32_t(-1);
      uint32_t bindTsSmp   = uint32_t(-1);
      uint32_t bindMs      = uint32_t(-1);
      uint32_t bindMsSmp   = uint32_t(-1);
      uint32_t bindFs      = uint32_t(-1);
      uint32_t bindFsSmp   = uint32_t(-1);
      uint32_t bindCs      = uint32_t(-1);
      uint32_t bindCsSmp   = uint32_t(-1);
      uint32_t mslPushSize = 0;
      };

    MtPipelineLay(const std::vector<Binding> **sh, size_t cnt, ShaderReflection::Stage bufferSizeBuffer);

    std::vector<Binding>          lay;
    std::vector<MTLBind>          bind;

    ShaderReflection::PushBlock   pb;
    MTLBind                       bindPush;

    uint32_t                      vboIndex = 0;
    ShaderReflection::Stage       bufferSizeBuffer = ShaderReflection::Stage::None;
  };

}
}
