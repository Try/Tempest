#include "mtpipeline.h"

#include "mtdevice.h"
#include "mtshader.h"
#include "mtfbolayout.h"
#include "mtpipelinelay.h"

using namespace Tempest;
using namespace Tempest::Detail;

MtPipeline::MtPipeline(MtDevice &d, Topology tp,
                       const RenderState &rs, size_t stride,
                       const MtPipelineLay& lay,
                       const MtShader &vert, const MtShader &frag)
  :lay(&lay), device(d), rs(rs), vert(&vert), frag(&frag) {
  cullMode = nativeFormat(rs.cullFaceMode());
  topology = nativeFormat(tp);

  MTLDepthStencilDescriptor *ddesc = [MTLDepthStencilDescriptor new];
  ddesc.depthCompareFunction = nativeFormat(rs.zTestMode());
  ddesc.depthWriteEnabled    = rs.isZWriteEnabled() ? YES : NO;
  depthStZ   = [d.impl newDepthStencilStateWithDescriptor:ddesc];

  ddesc.depthCompareFunction = MTLCompareFunctionAlways;
  ddesc.depthWriteEnabled    = NO;
  depthStNoZ = [d.impl newDepthStencilStateWithDescriptor:ddesc];

  vdesc = [MTLVertexDescriptor new];
  size_t offset = 0;
  for(size_t i=0; i<vert.vdecl.size(); ++i) {
    const auto& v = vert.vdecl[i];

    vdesc.attributes[i].bufferIndex = lay.vboIndex;
    vdesc.attributes[i].offset      = offset;
    vdesc.attributes[i].format      = nativeFormat(v);

    offset += Decl::size(v);
    }
  vdesc.layouts[lay.vboIndex].stride       = stride;
  vdesc.layouts[lay.vboIndex].stepRate     = 1;
  vdesc.layouts[lay.vboIndex].stepFunction = MTLVertexStepFunctionPerVertex;

  pdesc = [MTLRenderPipelineDescriptor new];
  pdesc.sampleCount          = 1;
  pdesc.vertexFunction       = vert.impl;
  pdesc.fragmentFunction     = frag.impl;
  pdesc.vertexDescriptor     = vdesc;
  pdesc.rasterizationEnabled = rs.isRasterDiscardEnabled() ? NO : YES; // TODO: test it

  for(size_t i=0; i<lay.lay.size(); ++i) {
    auto& l = lay.lay[i];
    auto& m = lay.bind[i];
    if(l.stage==0)
      continue;
    if(l.cls!=ShaderReflection::Ubo && l.cls!=ShaderReflection::SsboR && l.cls!=ShaderReflection::SsboRW)
      continue;
    MTLMutability mu = (l.cls==ShaderReflection::SsboRW) ? MTLMutabilityMutable: MTLMutabilityImmutable;

    if(m.bindVs!=uint32_t(-1))
      pdesc.vertexBuffers[m.bindVs].mutability = mu;
    if(m.bindFs!=uint32_t(-1))
      pdesc.fragmentBuffers[m.bindFs].mutability = mu;
    }
  }

MtPipeline::~MtPipeline() {
  for(auto& i:instance)
    if(i.pso!=nil)
      [i.pso release];
  [vdesc release];
  [pdesc release];
  }

MtPipeline::Inst& MtPipeline::inst(const MtFboLayout& lay) {
  for(auto& i:instance)
    if(i.fbo.equals(lay))
      return i;
  instance.emplace_back();

  Inst& ix = instance.back();
  ix.fbo = lay;

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

  id dev = device.impl;

  NSError* error = nil;
  ix.pso = [dev newRenderPipelineStateWithDescriptor:pdesc error:&error];
  mtAssert(ix.pso,error);
  return ix;
  }


MtCompPipeline::MtCompPipeline(MtDevice &device, const MtPipelineLay& lay, const MtShader &sh)
  :lay(&lay) {
  id dev = device.impl;

  MTLComputePipelineDescriptor* pdesc = [MTLComputePipelineDescriptor new];
  pdesc.computeFunction                                 = sh.impl;
  // pdesc.threadGroupSizeIsMultipleOfThreadExecutionWidth = YES;

  for(size_t i=0; i<lay.lay.size(); ++i) {
    auto& l = lay.lay[i];
    auto& m = lay.bind[i];
    if(l.stage==0)
      continue;
    if(l.cls!=ShaderReflection::Ubo && l.cls!=ShaderReflection::SsboR && l.cls!=ShaderReflection::SsboRW)
      continue;
    MTLMutability mu = (l.cls==ShaderReflection::SsboRW) ? MTLMutabilityMutable: MTLMutabilityImmutable;
    if(m.bindCs!=uint32_t(-1))
      pdesc.buffers[m.bindCs].mutability = mu;
    }

  NSError* error = nil;
  impl = [dev newComputePipelineStateWithDescriptor:pdesc
              options:MTLPipelineOptionNone
              reflection:nil
              error:&error];
  [pdesc release];

  mtAssert(impl,error);
  }
