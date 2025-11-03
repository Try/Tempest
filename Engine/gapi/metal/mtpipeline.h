#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <Tempest/RenderState>
#include <Metal/Metal.hpp>
#include <list>

#include "gapi/metal/mtpipelinelay.h"
#include "gapi/metal/mtfbolayout.h"
#include "gapi/metal/mtshader.h"
#include "gapi/shaderreflection.h"
#include "utility/spinlock.h"
#include "nsptr.h"

namespace Tempest {
namespace Detail {

class MtDevice;
class MtShader;
class MtFboLayout;

class MtPipeline : public AbstractGraphicsApi::Pipeline {
  public:
    MtPipeline(MtDevice &d, Topology tp, const Tempest::RenderState& rs,
               const MtShader*const* sh, size_t cnt);
    ~MtPipeline();

    IVec3  workGroupSize() const override;
    size_t sizeofBuffer(size_t id, size_t arraylen) const override;

    MTL::RenderPipelineState& inst(const MtFboLayout &lay, size_t stride);

    MtPipelineLay                     lay;

    NsPtr<MTL::DepthStencilState>     depthStZ;
    NsPtr<MTL::DepthStencilState>     depthStNoZ;

    MTL::CullMode                     cullMode = MTL::CullModeNone;
    MTL::PrimitiveType                topology = MTL::PrimitiveTypeTriangle;
    size_t                            defaultStride = 0;
    bool                              isTesselation = false;

    MTL::Size                         localSize     = {};
    MTL::Size                         localSizeMesh = {};

  private:
    struct Inst {
      NsPtr<MTL::RenderPipelineState> pso;
      size_t                          stride=0;
      MtFboLayout                     fbo;
      };

    const MtShader*                      findShader(ShaderReflection::Stage sh) const;
    void                                 mkVertexPso(const MtPipelineLay& lay);
    void                                 mkMeshPso(const MtPipelineLay& lay);

    MtDevice&                            device;
    Tempest::RenderState                 rs;

    NsPtr<MTL::VertexDescriptor>         vdesc;
    NsPtr<MTL::RenderPipelineDescriptor> pdesc;

    NsPtr<MTL::MeshRenderPipelineDescriptor> mdesc;

    uint32_t                             vboIndex = 0;
    DSharedPtr<const MtShader*>          modules[5] = {};

    SpinLock                             sync;
    std::list<Inst>                      instance;
  };

class MtCompPipeline : public AbstractGraphicsApi::CompPipeline {
  public:
    MtCompPipeline(MtDevice &d, const MtShader& sh);

    IVec3  workGroupSize() const override;
    size_t sizeofBuffer(size_t id, size_t arraylen) const override;

    NsPtr<MTL::ComputePipelineState> impl;
    MtPipelineLay                    lay;
    MTL::Size                        localSize = {};
  };

}
}

