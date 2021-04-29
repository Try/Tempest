#include "mtpipeline.h"

#include "mtdevice.h"
#include "mtshader.h"
#include "mtframebuffer.h"
#include "mtfbolayout.h"

using namespace Tempest;
using namespace Tempest::Detail;

MtPipeline::MtPipeline(MtDevice &d,
                       const RenderState &rs, size_t stride,
                       const MtShader &vert, const MtShader &frag)
  :device(d), rs(rs), vert(&vert), frag(&frag) {
  cullMode = nativeFormat(rs.cullFaceMode());

  id dev = d.impl.get();

  MTLDepthStencilDescriptor *ddesc = [[MTLDepthStencilDescriptor alloc] init];
  ddesc.depthCompareFunction = nativeFormat(rs.zTestMode());
  ddesc.depthWriteEnabled    = rs.isZWriteEnabled() ? YES : NO;
  depthStZ = [dev newDepthStencilStateWithDescriptor:ddesc];

  ddesc.depthWriteEnabled = NO;
  depthStNoZ = [dev newDepthStencilStateWithDescriptor:ddesc];

  vdesc = [[MTLVertexDescriptor alloc] init];
  size_t offset = 0;
  for(size_t i=0; i<vert.vdecl.size(); ++i) {
    const auto& v = vert.vdecl[i];

    vdesc.attributes[i].bufferIndex = 0;
    vdesc.attributes[i].offset      = offset;
    vdesc.attributes[i].format      = nativeFormat(v);

    offset += Decl::size(v);
    }
  vdesc.layouts[0].stride       = stride;
  vdesc.layouts[0].stepRate     = 1;
  vdesc.layouts[0].stepFunction = MTLVertexStepFunctionPerVertex;

  pdesc = [[MTLRenderPipelineDescriptor alloc] init];
  pdesc.sampleCount          = 1;
  pdesc.vertexFunction       = vert.impl.get();
  pdesc.fragmentFunction     = frag.impl.get();
  pdesc.vertexDescriptor     = vdesc;
  pdesc.rasterizationEnabled = rs.isRasterDiscardEnabled() ? NO : YES; // TODO: test it
  }

MtPipeline::~MtPipeline() {
  [vdesc release];
  }

MtPipeline::Inst& MtPipeline::inst(const MtFboLayout& lay) {
  for(auto& i:instance)
    if(i.fbo.handler->equals(lay))
      return i;
  instance.emplace_back();

  Inst& ix = instance.back();
  ix.fbo = DSharedPtr<const MtFboLayout*>(&lay);

  for(size_t i=0; i<lay.numColors; ++i) {
    pdesc.colorAttachments[i].pixelFormat                 = lay.colorFormat[i];

    pdesc.colorAttachments[i].blendingEnabled             = rs.hasBlend() ? YES : NO;
    pdesc.colorAttachments[i].rgbBlendOperation           = nativeFormat(rs.blendOperation());
    pdesc.colorAttachments[i].alphaBlendOperation         = pdesc.colorAttachments[i].rgbBlendOperation;
    pdesc.colorAttachments[i].destinationRGBBlendFactor   = nativeFormat(rs.blendDest());
    pdesc.colorAttachments[i].destinationAlphaBlendFactor = pdesc.colorAttachments[i].destinationRGBBlendFactor;
    pdesc.colorAttachments[i].sourceRGBBlendFactor        = nativeFormat(rs.blendSource());
    pdesc.colorAttachments[i].sourceAlphaBlendFactor      = pdesc.colorAttachments[i].sourceRGBBlendFactor;
    }
  pdesc.depthAttachmentPixelFormat = lay.depthFormat;

  id dev = device.impl.get();

  NSError* error = nil;
  ix.pso = NsPtr((__bridge void*)([dev newRenderPipelineStateWithDescriptor:pdesc error:&error]));
  if(ix.pso.ptr==nullptr)
    ; // TODO: exceptions
  return ix;
  }