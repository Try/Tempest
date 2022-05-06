#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <Tempest/RenderState>

#include "utility/spinlock.h"
#include "mtfbolayout.h"
#include "mtshader.h"
#include "gapi/shaderreflection.h"

#import  <Metal/MTLRenderCommandEncoder.h>
#import  <Metal/MTLVertexDescriptor.h>
#import  <Metal/MTLDepthStencil.h>
#import  <Metal/MTLRenderPipeline.h>
#import  <Metal/MTLComputePipeline.h>

#include <list>

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
      id<MTLRenderPipelineState> pso;
      MtFboLayout                fbo;
      };
    Inst& inst(const MtFboLayout &lay);

    DSharedPtr<const MtPipelineLay*> lay;

    id<MTLDepthStencilState> depthStZ;
    id<MTLDepthStencilState> depthStNoZ;

    MTLCullMode              cullMode = MTLCullModeNone;
    MTLPrimitiveType         topology = MTLPrimitiveTypeTriangle;
    bool                     isTesselation = false;

  private:
    const MtShader*          findShader(ShaderReflection::Stage sh) const;

    MtDevice&            device;
    Tempest::RenderState rs;

    MTLVertexDescriptor*         vdesc = nil;
    MTLRenderPipelineDescriptor* pdesc = nil;
    DSharedPtr<const MtShader*>  modules[5] = {};
    SpinLock        sync;
    std::list<Inst> instance;
  };

class MtCompPipeline : public AbstractGraphicsApi::CompPipeline {
  public:
    MtCompPipeline(MtDevice &d, const MtPipelineLay& lay, const MtShader& sh);

    id<MTLComputePipelineState>      impl;
    DSharedPtr<const MtPipelineLay*> lay;
  };

}
}

