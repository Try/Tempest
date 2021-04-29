#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <Tempest/RenderState>

#include "utility/spinlock.h"
#include "mtfbolayout.h"
#include "nsptr.h"

#import  <Metal/MTLRenderCommandEncoder.h>
#import  <Metal/MTLVertexDescriptor.h>
#import  <Metal/MTLDepthStencil.h>
#import  <Metal/MTLRenderPipeline.h>
#include <list>

namespace Tempest {
namespace Detail {

class MtDevice;
class MtShader;
class MtFboLayout;

class MtPipeline : public AbstractGraphicsApi::Pipeline {
  public:
    MtPipeline(MtDevice &d, const Tempest::RenderState& rs,
               size_t stride,
               const MtShader &vert,
               const MtShader &frag);
    ~MtPipeline();

    struct Inst {
      NsPtr                          pso;
      DSharedPtr<const MtFboLayout*> fbo;
      };
    Inst& inst(const MtFboLayout &lay);

    id<MTLDepthStencilState> depthStZ;
    id<MTLDepthStencilState> depthStNoZ;

    MTLCullMode              cullMode = MTLCullModeNone;

  private:
    MTLPrimitiveType     topology = MTLPrimitiveTypeTriangle;
    MtDevice&            device;
    Tempest::RenderState rs;

    MTLVertexDescriptor*         vdesc = nil;
    MTLRenderPipelineDescriptor* pdesc = nil;

    const MtShader* vert = nullptr;
    const MtShader* frag = nullptr;

    SpinLock        sync;
    std::list<Inst> instance;
  };

}
}

