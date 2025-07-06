#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <Metal/Metal.hpp>

#include "utility/smallarray.h"
#include "gapi/shaderreflection.h"
#include "mtbuffer.h"

namespace Tempest {
namespace Detail {

class MtDevice;

class MtDescriptorArray : public AbstractGraphicsApi::DescArray {
  public:
    MtDescriptorArray(MtDevice& dev, AbstractGraphicsApi::Texture** tex, size_t cnt, uint32_t mipLevel, const Sampler& smp);
    MtDescriptorArray(MtDevice& dev, AbstractGraphicsApi::Texture** tex, size_t cnt, uint32_t mipLevel);
    MtDescriptorArray(MtDevice& dev, AbstractGraphicsApi::Buffer**  buf, size_t cnt);
    ~MtDescriptorArray();

    size_t       size() const;
    MTL::Buffer& data() { return *argsBuf.impl.get(); }

    void useResource(MTL::ComputeCommandEncoder& cmd, ShaderReflection::Stage st) const;
    void useResource(MTL::RenderCommandEncoder&  cmd, ShaderReflection::Stage st) const;

  private:
    void fillBuffer(MtDevice& dev, AbstractGraphicsApi::Texture** tex, size_t cnt, uint32_t mipLevel, const Sampler* smp);
    void fillBuffer(MtDevice& dev, AbstractGraphicsApi::Buffer**  buf, size_t cnt);

    template<class Enc>
    void implUseResource(Enc&  cmd, ShaderReflection::Stage st) const;

    void implUseResource(MTL::ComputeCommandEncoder& cmd,
                         const MTL::Resource* const resources[], NS::UInteger count, MTL::ResourceUsage usage, MTL::RenderStages stages) const;
    void implUseResource(MTL::RenderCommandEncoder& cmd,
                         const MTL::Resource* const resources[], NS::UInteger count, MTL::ResourceUsage usage, MTL::RenderStages stages) const;

    MtBuffer                    argsBuf;
    std::vector<MTL::Resource*> args;
    size_t                      cnt  = 0;
  };
}
}


