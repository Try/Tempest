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

    void set(size_t id, AbstractGraphicsApi::Texture* tex, const Sampler& smp, uint32_t mipLevel) override;
    void set(size_t id, AbstractGraphicsApi::Buffer*  buf, size_t offset) override;
    void set(size_t id, const Sampler& smp) override;
    void set(size_t id, AbstractGraphicsApi::AccelerationStructure*) override;
    void set(size_t id, AbstractGraphicsApi::Texture** tex, size_t cnt, const Sampler& smp, uint32_t mipLevel) override;
    void set(size_t id, AbstractGraphicsApi::Buffer**  buf, size_t cnt) override;

    void fillBufferSizeBuffer(uint32_t* ret, ShaderReflection::Stage stage, const MtPipelineLay& lay);
    void useResource(MTL::ComputeCommandEncoder& cmd);
    void useResource(MTL::RenderCommandEncoder&  cmd);

    struct Desc {
      void*              val     = nullptr;
      void*              valAtom = nullptr;
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

    void implUseResource(MTL::ComputeCommandEncoder& cmd,
                         const MTL::Resource* const resources[], NS::UInteger count, MTL::ResourceUsage usage, MTL::RenderStages stages);
    void implUseResource(MTL::RenderCommandEncoder& cmd,
                         const MTL::Resource* const resources[], NS::UInteger count, MTL::ResourceUsage usage, MTL::RenderStages stages);
  };

class MtDescriptorArray2 : public AbstractGraphicsApi::DescArray {
  public:
    MtDescriptorArray2(MtDevice& dev, AbstractGraphicsApi::Texture** tex, size_t cnt, uint32_t mipLevel, const Sampler& smp);
    MtDescriptorArray2(MtDevice& dev, AbstractGraphicsApi::Texture** tex, size_t cnt, uint32_t mipLevel);
    MtDescriptorArray2(MtDevice& dev, AbstractGraphicsApi::Buffer**  buf, size_t cnt);
    ~MtDescriptorArray2();

    size_t       size() const;
    MTL::Buffer& data() { return *argsBuf.impl.get(); }

    void useResource(MTL::ComputeCommandEncoder& cmd, ShaderReflection::Stage st);
    void useResource(MTL::RenderCommandEncoder&  cmd, ShaderReflection::Stage st);

  private:
    void fillBuffer(MtDevice& dev, AbstractGraphicsApi::Texture** tex, size_t cnt, uint32_t mipLevel, const Sampler* smp);
    void fillBuffer(MtDevice& dev, AbstractGraphicsApi::Buffer**  buf, size_t cnt);

    template<class Enc>
    void implUseResource(Enc&  cmd, ShaderReflection::Stage st);

    void implUseResource(MTL::ComputeCommandEncoder& cmd,
                         const MTL::Resource* const resources[], NS::UInteger count, MTL::ResourceUsage usage, MTL::RenderStages stages);
    void implUseResource(MTL::RenderCommandEncoder& cmd,
                         const MTL::Resource* const resources[], NS::UInteger count, MTL::ResourceUsage usage, MTL::RenderStages stages);

    MtBuffer                    argsBuf;
    std::vector<MTL::Resource*> args;
    size_t                      cnt  = 0;
  };
}
}


