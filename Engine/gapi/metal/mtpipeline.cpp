#if defined(TEMPEST_BUILD_METAL)

#include "mtpipeline.h"

#include <Tempest/Log>

#include "mtdevice.h"
#include "mtshader.h"
#include "mtfbolayout.h"
#include "mtpipelinelay.h"

using namespace Tempest;
using namespace Tempest::Detail;

MtPipeline::MtPipeline(MtDevice &d, Topology tp,
                       const RenderState &rs,
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

  auto mesh = findShader(ShaderReflection::Mesh);
  if(mesh!=nullptr) {
    mkMeshPso(lay);
    } else {
    mkVertexPso(lay);
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
    if(m.bindFs!=uint32_t(-1) && pdesc!=nullptr)
      pdesc->fragmentBuffers()->object(m.bindFs)->setMutability(mu);

    if(m.bindTs!=uint32_t(-1))
      mdesc->objectBuffers()->object(m.bindTs)->setMutability(mu);
    if(m.bindMs!=uint32_t(-1))
      mdesc->meshBuffers()->object(m.bindMs)->setMutability(mu);
    if(m.bindFs!=uint32_t(-1) && mdesc!=nullptr)
      mdesc->fragmentBuffers()->object(m.bindFs)->setMutability(mu);
    }
  }

MtPipeline::~MtPipeline() {
  }

IVec3 MtPipeline::workGroupSize() const {
  if(localSize.width>0)
    return IVec3(int(localSize.width), int(localSize.height), int(localSize.depth));
  return IVec3(int(localSizeMesh.width), int(localSizeMesh.height), int(localSizeMesh.depth));
  }

MTL::RenderPipelineState& MtPipeline::inst(const MtFboLayout& lay, size_t stride) {
  std::lock_guard<SpinLock> guard(sync);

  for(auto& i:instance)
    if(i.stride==stride && i.fbo.equals(lay))
      return *i.pso.get();
  instance.emplace_back();

  Inst& ix = instance.back();
  ix.stride = stride;
  ix.fbo    = lay;

  MTL::RenderPipelineColorAttachmentDescriptorArray* att = nullptr;
  if(pdesc!=nullptr)
    att = pdesc->colorAttachments();
  if(mdesc!=nullptr)
    att = mdesc->colorAttachments();

  for(size_t i=0; i<lay.numColors; ++i) {
    auto clr = att->object(i);
    clr->setPixelFormat(lay.colorFormat[i]);
    clr->setBlendingEnabled(rs.hasBlend());
    clr->setRgbBlendOperation          (nativeFormat(rs.blendOperation()));
    clr->setAlphaBlendOperation        (nativeFormat(rs.blendOperation()));
    clr->setDestinationRGBBlendFactor  (nativeFormat(rs.blendDest()));
    clr->setDestinationAlphaBlendFactor(nativeFormat(rs.blendDest()));
    clr->setSourceRGBBlendFactor       (nativeFormat(rs.blendSource()));
    clr->setSourceAlphaBlendFactor     (nativeFormat(rs.blendSource()));
    }

  if(mdesc!=nullptr) {
    mdesc->setDepthAttachmentPixelFormat(lay.depthFormat);

    NS::Error* error = nullptr;
    ix.pso = NsPtr<MTL::RenderPipelineState>(device.impl->newRenderPipelineState(mdesc.get(),MTL::PipelineOptionNone,nullptr,&error));
    mtAssert(ix.pso.get(),error);
    } else {
    vdesc->layouts()->object(vboIndex)->setStride(stride);
    pdesc->setDepthAttachmentPixelFormat(lay.depthFormat);

    NS::Error* error = nullptr;
    ix.pso = NsPtr<MTL::RenderPipelineState>(device.impl->newRenderPipelineState(pdesc.get(),&error));
    mtAssert(ix.pso.get(),error);
    }

  return *ix.pso.get();
  }

const MtShader* MtPipeline::findShader(ShaderReflection::Stage sh) const {
  for(auto& i:modules)
    if(i.handler!=nullptr && i.handler->stage==sh)
      return i.handler;
  return nullptr;
  }

void MtPipeline::mkVertexPso(const MtPipelineLay& lay) {
  auto vert = findShader(ShaderReflection::Vertex);
  auto tesc = findShader(ShaderReflection::Control);
  auto tese = findShader(ShaderReflection::Evaluate);
  auto frag = findShader(ShaderReflection::Fragment);

  size_t offset = 0;
  vboIndex = lay.vboIndex;
  vdesc = NsPtr<MTL::VertexDescriptor>::init();
  vdesc->retain();
  for(size_t i=0; i<vert->vdecl.size(); ++i) {
    const auto& v = vert->vdecl[i];
    vdesc->attributes()->object(i)->setBufferIndex(vboIndex);
    vdesc->attributes()->object(i)->setOffset(offset);
    vdesc->attributes()->object(i)->setFormat(nativeFormat(v));
    offset += Decl::size(v);
    }
  defaultStride = offset;

  vdesc->layouts()->object(vboIndex)->setStride(defaultStride);
  vdesc->layouts()->object(vboIndex)->setStepRate(1);
  vdesc->layouts()->object(vboIndex)->setStepFunction(MTL::VertexStepFunctionPerVertex);

  pdesc = NsPtr<MTL::RenderPipelineDescriptor>::init();
  pdesc->retain();
  pdesc->setSampleCount(1); // deprecated
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
  }

void MtPipeline::mkMeshPso(const MtPipelineLay& lay) {
  auto task = findShader(ShaderReflection::Task);
  auto mesh = findShader(ShaderReflection::Mesh);
  auto frag = findShader(ShaderReflection::Fragment);

  if(task!=nullptr) {
    localSize     = task->comp.localSize;
    localSizeMesh = mesh->comp.localSize;
    } else {
    localSize     = MTL::Size(0,0,0);
    localSizeMesh = mesh->comp.localSize;
    }

  mdesc = NsPtr<MTL::MeshRenderPipelineDescriptor>::init();
  mdesc->retain();
  //mdesc->setSampleCount(1);
  mdesc->setObjectFunction((task!=nullptr) ? task->impl.get() : nullptr);
  mdesc->setMeshFunction(mesh->impl.get());
  mdesc->setFragmentFunction(frag->impl.get());
  mdesc->setRasterizationEnabled(!rs.isRasterDiscardEnabled()); // TODO: test

  if(mesh!=nullptr) {
    mdesc->setMaxTotalThreadsPerMeshThreadgroup(localSizeMesh.width*localSizeMesh.height*localSizeMesh.depth);
    //mdesc->setMeshThreadgroupSizeIsMultipleOfThreadExecutionWidth(true);
    }

  if(task!=nullptr) {
    mdesc->setMaxTotalThreadsPerObjectThreadgroup(localSize.width*localSize.height*localSize.depth);
    //mdesc->setObjectThreadgroupSizeIsMultipleOfThreadExecutionWidth(true);
    }
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
  if(error!=nullptr) {
    const char* e = error->localizedDescription()->utf8String();
#if !defined(NDEBUG)
    Log::d("NSError: \"",e,"\"");
#endif
    throw std::system_error(Tempest::GraphicsErrc::InvalidShaderModule, "\""+sh.dbg.source + "\": " + e);
    }
  }

IVec3 MtCompPipeline::workGroupSize() const {
  return IVec3{int(localSize.width),int(localSize.height),int(localSize.depth)};
  }

#endif
