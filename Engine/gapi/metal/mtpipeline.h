#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <Tempest/RenderState>
#include <Metal/Metal.hpp>
#include <list>

#include "utility/spinlock.h"
#include "mtfbolayout.h"
#include "mtshader.h"
#include "gapi/shaderreflection.h"
#include "nsptr.h"

namespace Tempest {
namespace Detail {

class MtDevice;
class MtShader;
class MtFboLayout;
class MtPipelineLay;

class MtPipeline : public AbstractGraphicsApi::Pipeline {
  public:
    MtPipeline(MtDevice &d, Topology tp, const Tempest::RenderState& rs,
               size_t stride, const MtPipelineLay& lay,
               const MtShader*const* sh, size_t cnt);
    ~MtPipeline();

    struct Inst {
      NsPtr<MTL::RenderPipelineState> pso;
      MtFboLayout                     fbo;
      };
    Inst& inst(const MtFboLayout &lay);

    DSharedPtr<const MtPipelineLay*>  lay;

    NsPtr<MTL::DepthStencilState>     depthStZ;
    NsPtr<MTL::DepthStencilState>     depthStNoZ;

    MTL::CullMode                     cullMode = MTL::CullModeNone;
    MTL::PrimitiveType                topology = MTL::PrimitiveTypeTriangle;
    bool                              isTesselation = false;

  private:
    const MtShader*                   findShader(ShaderReflection::Stage sh) const;

    MtDevice&                            device;
    Tempest::RenderState                 rs;

    NsPtr<MTL::VertexDescriptor>         vdesc;
    NsPtr<MTL::RenderPipelineDescriptor> pdesc;
    DSharedPtr<const MtShader*>          modules[5] = {};
    SpinLock                             sync;
    std::list<Inst>                      instance;
  };

class MtCompPipeline : public AbstractGraphicsApi::CompPipeline {
  public:
    MtCompPipeline(MtDevice &d, const MtPipelineLay& lay, const MtShader& sh);

    IVec3 workGroupSize() const;

    NsPtr<MTL::ComputePipelineState> impl;
    DSharedPtr<const MtPipelineLay*> lay;
    MTL::Size                        localSize = {};
  };

}
}

