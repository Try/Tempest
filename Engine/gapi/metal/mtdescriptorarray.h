#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <Metal/Metal.hpp>

#include "utility/smallarray.h"
#include "gapi/shaderreflection.h"
#include "mtbuffer.h"

namespace Tempest {
namespace Detail {

class MtPipelineLay;
class MtDevice;
class MtTopAccelerationStructure;

class MtDescriptorArray : public AbstractGraphicsApi::Desc {
  public:
    MtDescriptorArray(MtDevice& dev, const MtPipelineLay &lay);

    void set    (size_t id, AbstractGraphicsApi::Texture* tex, const Sampler& smp, uint32_t mipLevel) override;
    void set    (size_t id, AbstractGraphicsApi::Buffer*  buf, size_t offset) override;
    void set    (size_t id, const Sampler& smp) override;
    void setTlas(size_t,AbstractGraphicsApi::AccelerationStructure*) override;

    void set    (size_t id, AbstractGraphicsApi::Texture** tex, size_t cnt, const Sampler& smp, uint32_t mipLevel) override;
    void set    (size_t id, AbstractGraphicsApi::Buffer**  buf, size_t cnt) override;

    void fillBufferSizeBuffer(uint32_t* ret, ShaderReflection::Stage stage);
    void useResource(MTL::ComputeCommandEncoder& cmd);
    void useResource(MTL::RenderCommandEncoder&  cmd);

    struct Desc {
      void*              val     = nullptr;
      MTL::SamplerState* sampler = nullptr;
      size_t             offset  = 0;
      size_t             length  = 0;

      MtTopAccelerationStructure* tlas = nullptr;

      MtBuffer           argsBuf;
      std::vector<MTL::Resource*> args;
      };

    MtDevice&                        dev;
    SmallArray<Desc,16>              desc;
    DSharedPtr<const MtPipelineLay*> lay;

    ShaderReflection::Stage          bufferSizeBuffer = ShaderReflection::Stage::None;

  private:
    template<class Enc>
    void implUseResource(Enc&  cmd);
  };

}
}


