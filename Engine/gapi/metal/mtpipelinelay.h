#pragma once

#include <Tempest/AbstractGraphicsApi>

#include "gapi/shaderreflection.h"

namespace Tempest {
namespace Detail {

class MtShader;

class MtPipelineLay : public AbstractGraphicsApi::PipelineLay {
  public:
    using Binding = ShaderReflection::Binding;

    MtPipelineLay(const std::vector<Binding>& cs);
    MtPipelineLay(const std::vector<Binding> **sh, size_t cnt);

    size_t descriptorsCount() override;

    void adjustSsboBindings();

    std::vector<Binding>          lay;
    ShaderReflection::PushBlock   pb;
    bool                          hasSSBO = false;
  };

}
}
