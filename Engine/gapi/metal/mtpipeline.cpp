#if defined(TEMPEST_BUILD_METAL)

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
                       const MtShader* const * sh, size_t count)
  :lay(&lay), device(d), rs(rs) {
  for(size_t i=0; i<count; ++i)
    if(sh[i]!=nullptr)
      modules[i] = Detail::DSharedPtr<const MtShader*>{sh[i]};

  cullMode = nativeFormat(rs.cullFaceMode());
  topology = nativeFormat(tp);

  auto ddesc = NsPtr<MTL::DepthStencilDescriptor>::init();
  ddesc->setDepthCompareFunction(nativeFormat(rs.zTestMode()));
  ddesc->setDepthWriteEnabled(rs.isZWriteEnabled());
  depthStZ = NsPtr<MTL::DepthStencilState>(d.impl->newDepthStencilState(ddesc.get()));

  ddesc->setDepthCompareFunction(MTL::CompareFunctionAlways);
  ddesc->setDepthWriteEnabled(false);
  depthStNoZ = NsPtr<MTL::DepthStencilState>(d.impl->newDepthStencilState(ddesc.get()));

  auto vert = findShader(ShaderReflection::Vertex);
  auto tesc = findShader(ShaderReflection::Control);
  auto tese = findShader(ShaderReflection::Evaluate);
  auto frag = findShader(ShaderReflection::Fragment);

  size_t offset = 0;
  vdesc = NsPtr<MTL::VertexDescriptor>::init();
  vdesc->retain();
  for(size_t i=0; i<vert->vdecl.size(); ++i) {
    const auto& v = vert->vdecl[i];
    vdesc->attributes()->object(i)->setBufferIndex(lay.vboIndex);
    vdesc->attributes()->object(i)->setOffset(offset);
    vdesc->attributes()->object(i)->setFormat(nativeFormat(v));
    offset += Decl::size(v);
    }
  vdesc->layouts()->object(lay.vboIndex)->setStride(stride);
  vdesc->layouts()->object(lay.vboIndex)->setStepRate(1);
  vdesc->layouts()->object(lay.vboIndex)->setStepFunction(MTL::VertexStepFunctionPerVertex);

  pdesc = NsPtr<MTL::RenderPipelineDescriptor>::init();
  pdesc->retain();
  pdesc->setSampleCount(1);
  pdesc->setVertexFunction(vert->impl.get());
  pdesc->setFragmentFunction(frag->impl.get());
  pdesc->setRasterizationEnabled(!rs.isRasterDiscardEnabled()); // TODO: test it

  if(tesc!=nullptr && tese!=nullptr) {
    pdesc->setMaxTessellationFactor(16);
    pdesc->setTessellationFactorScaleEnabled(false);
    pdesc->setTessellationFactorStepFunction(MTL::TessellationFactorStepFunctionConstant);
    pdesc->setTessellationOutputWindingOrder(tese->tese.winding);
    pdesc->setTessellationPartitionMode(tese->tese.partition);
    pdesc->setVertexFunction(tese->impl.get());

    vdesc->layouts()->object(lay.vboIndex)->setStepFunction(MTL::VertexStepFunctionPerPatch);
    pdesc->setVertexDescriptor(vdesc.get());
    isTesselation = true;
    } else {
    pdesc->setVertexDescriptor(vdesc.get());
    }

  for(size_t i=0; i<lay.lay.size(); ++i) {
    auto& l = lay.lay[i];
    auto& m = lay.bind[i];
    if(l.stage==0)
      continue;
    if(l.cls!=ShaderReflection::Ubo && l.cls!=ShaderReflection::SsboR && l.cls!=ShaderReflection::SsboRW)
      continue;
    MTL::Mutability mu = (l.cls==ShaderReflection::SsboRW) ? MTL::MutabilityMutable: MTL::MutabilityImmutable;

    if(m.bindVs!=uint32_t(-1))
      pdesc->vertexBuffers()->object(m.bindVs)->setMutability(mu);
    if(m.bindFs!=uint32_t(-1))
      pdesc->fragmentBuffers()->object(m.bindFs)->setMutability(mu);
    }
  }

MtPipeline::~MtPipeline() {
  }

MtPipeline::Inst& MtPipeline::inst(const MtFboLayout& lay) {
  for(auto& i:instance)
    if(i.fbo.equals(lay))
      return i;
  instance.emplace_back();

  Inst& ix = instance.back();
  ix.fbo = lay;

  for(size_t i=0; i<lay.numColors; ++i) {
    auto clr = pdesc->colorAttachments()->object(i);
    clr->setPixelFormat(lay.colorFormat[i]);
    clr->setBlendingEnabled(rs.hasBlend());
    clr->setRgbBlendOperation          (nativeFormat(rs.blendOperation()));
    clr->setAlphaBlendOperation        (nativeFormat(rs.blendOperation()));
    clr->setDestinationRGBBlendFactor  (nativeFormat(rs.blendDest()));
    clr->setDestinationAlphaBlendFactor(nativeFormat(rs.blendDest()));
    clr->setSourceRGBBlendFactor       (nativeFormat(rs.blendSource()));
    clr->setSourceAlphaBlendFactor     (nativeFormat(rs.blendSource()));
    }
  pdesc->setDepthAttachmentPixelFormat(lay.depthFormat);

  NS::Error* error = nullptr;
  ix.pso = NsPtr<MTL::RenderPipelineState>(device.impl->newRenderPipelineState(pdesc.get(),&error));
  mtAssert(ix.pso.get(),error);
  return ix;
  }

const MtShader* MtPipeline::findShader(ShaderReflection::Stage sh) const {
  for(auto& i:modules)
    if(i.handler!=nullptr && i.handler->stage==sh)
      return i.handler;
  return nullptr;
  }


MtCompPipeline::MtCompPipeline(MtDevice &device, const MtPipelineLay& lay, const MtShader &sh)
  :lay(&lay) {
  localSize = sh.comp.localSize;
  auto desc = NsPtr<MTL::ComputePipelineDescriptor>::init();
  desc->setComputeFunction(sh.impl.get());
  desc->setThreadGroupSizeIsMultipleOfThreadExecutionWidth(false); // it seems no way to guess correct with value
  desc->setMaxTotalThreadsPerThreadgroup(localSize.width*localSize.height*localSize.depth);

  for(size_t i=0; i<lay.lay.size(); ++i) {
    auto& l = lay.lay[i];
    auto& m = lay.bind[i];
    if(l.stage==0)
      continue;
    if(l.cls!=ShaderReflection::Ubo && l.cls!=ShaderReflection::SsboR && l.cls!=ShaderReflection::SsboRW)
      continue;
    MTL::Mutability mu = (l.cls==ShaderReflection::SsboRW) ? MTL::MutabilityMutable: MTL::MutabilityImmutable;
    if(m.bindCs!=uint32_t(-1))
      desc->buffers()->object(m.bindCs)->setMutability(mu);
    }

  NS::Error* error = nullptr;
  impl = NsPtr<MTL::ComputePipelineState>(device.impl->newComputePipelineState(desc.get(), MTL::PipelineOptionNone, nullptr, &error));
  mtAssert(impl.get(),error);
  }

IVec3 MtCompPipeline::workGroupSize() const {
  return IVec3{localSize.width,localSize.height,localSize.depth};
  }

#endif
