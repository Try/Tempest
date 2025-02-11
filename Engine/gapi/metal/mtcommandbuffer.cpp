#if defined(TEMPEST_BUILD_METAL)

#include "mtcommandbuffer.h"

#include "mtbuffer.h"
#include "mtdevice.h"
#include "mtpipeline.h"
#include "mtpipelinelay.h"
#include "mtdescriptorarray.h"
#include "mttexture.h"
#include "mtswapchain.h"
#include "mtaccelerationstructure.h"

using namespace Tempest;
using namespace Tempest::Detail;

static MTL::LoadAction mkLoadOp(const AccessOp m) {
  switch(m) {
    case AccessOp::Clear:    return MTL::LoadActionClear;
    case AccessOp::Preserve: return MTL::LoadActionLoad;
    case AccessOp::Discard:  return MTL::LoadActionDontCare;
    case AccessOp::Readonly: return MTL::LoadActionLoad;
    }
  return MTL::LoadActionDontCare;
  }

static MTL::StoreAction mkStoreOp(const AccessOp m) {
  switch(m) {
    case AccessOp::Clear:    return MTL::StoreActionStore;
    case AccessOp::Preserve: return MTL::StoreActionStore;
    case AccessOp::Discard:  return MTL::StoreActionDontCare;
    case AccessOp::Readonly: return MTL::StoreActionStore;
    }
  return MTL::StoreActionDontCare;
  }

static MTL::ClearColor mkClearColor(const Vec4& c) {
  MTL::ClearColor ret;
  ret.red   = c.x;
  ret.green = c.y;
  ret.blue  = c.z;
  ret.alpha = c.w;
  return ret;
  }

MtCommandBuffer::MtCommandBuffer(MtDevice& dev)
  : device(dev) {
  reset();
  }

MtCommandBuffer::~MtCommandBuffer() {
  }

bool MtCommandBuffer::isRecording() const {
  // FIXME
  return encDraw!=nullptr || encComp!=nullptr || encBlit!=nullptr;
  }

void MtCommandBuffer::begin() {
  reset();
  }

void MtCommandBuffer::end() {
  setEncoder(E_None,nullptr);
  }

void MtCommandBuffer::reset() {
  auto pool = NsPtr<NS::AutoreleasePool>::init();
  auto desc = NsPtr<MTL::CommandBufferDescriptor>::init();
  desc->setRetainedReferences(false);
  desc->setErrorOptions(MTL::CommandBufferErrorOptionEncoderExecutionStatus);

  impl = NsPtr<MTL::CommandBuffer>(device.queue->commandBuffer(desc.get()));
  impl->retain();
  }

void MtCommandBuffer::beginRendering(const AttachmentDesc* d, size_t descSize,
                                     uint32_t width, uint32_t height, const TextureFormat* frm,
                                     AbstractGraphicsApi::Texture** att,
                                     AbstractGraphicsApi::Swapchain** sw, const uint32_t* imgId) {
  curFbo.depthFormat = MTL::PixelFormatInvalid;
  curFbo.numColors   = 0;

  auto pool = NsPtr<NS::AutoreleasePool>::init();
  auto desc = NsPtr<MTL::RenderPassDescriptor>::init();
  for(size_t i=0; i<descSize; ++i) {
    auto& dx = d[i];
    if(dx.zbuffer!=nullptr) {
      auto& t = *reinterpret_cast<MtTexture*>(att[i]);
      desc->depthAttachment()->setTexture    (t.impl.get());
      desc->depthAttachment()->setLoadAction (mkLoadOp (dx.load));
      desc->depthAttachment()->setStoreAction(mkStoreOp(dx.store));
      desc->depthAttachment()->setClearDepth (dx.clear.x);
      curFbo.depthFormat = nativeFormat(frm[i]);
      continue;
      }
    auto clr = desc->colorAttachments()->object(i);
    if(sw[i]!=nullptr) {
      auto& s = *reinterpret_cast<MtSwapchain*>(sw[i]);
      clr->setTexture(s.img[imgId[i]].tex.get());
      curFbo.colorFormat[curFbo.numColors] = s.format();
      } else {
      auto& t = *reinterpret_cast<MtTexture*>(att[i]);
      clr->setTexture(t.impl.get());
      curFbo.colorFormat[curFbo.numColors] = nativeFormat(frm[i]);
      }
    clr->setLoadAction (mkLoadOp (dx.load));
    clr->setStoreAction(mkStoreOp(dx.store));
    clr->setClearColor (mkClearColor(dx.clear));
    ++curFbo.numColors;
    }

  setEncoder(E_Draw,desc.get());

  // [enc setFrontFacingWinding:MTLWindingCounterClockwise];
  setViewport(Rect(0,0,width,height));
  }

void MtCommandBuffer::endRendering() {
  setEncoder(E_None,nullptr);
  }

void MtCommandBuffer::barrier(const AbstractGraphicsApi::BarrierDesc* desc, size_t cnt) {
  // nop
  }

void MtCommandBuffer::setEncoder(MtCommandBuffer::EncType e, MTL::RenderPassDescriptor* desc) {
  if(encDraw!=nullptr && e!=E_Draw) {
    if(isDbgRegion) {
      encDraw->popDebugGroup();
      isDbgRegion = false;
      }
    encDraw->endEncoding();
    encDraw = NsPtr<MTL::RenderCommandEncoder>();
    bindings.durty = true;
    pushData.durty = true;
    }
  if(encComp!=nullptr && e!=E_Comp) {
    if(isDbgRegion) {
      encComp->popDebugGroup();
      isDbgRegion = false;
      }
    encComp->endEncoding();
    encComp = NsPtr<MTL::ComputeCommandEncoder>();
    bindings.durty = true;
    pushData.durty = true;
    }
  if(encBlit!=nullptr && e!=E_Blit) {
    if(isDbgRegion) {
      encBlit->popDebugGroup();
      isDbgRegion = false;
      }
    encBlit->endEncoding();
    encBlit = NsPtr<MTL::BlitCommandEncoder>();
    bindings.durty = true;
    pushData.durty = true;
    }

  switch(e) {
    case E_None: {
      break;
      }
    case E_Draw: {
      if(encDraw!=nullptr) {
        if(isDbgRegion) {
          encBlit->popDebugGroup();
          isDbgRegion = false;
          }
        encDraw->endEncoding();
        encDraw = NsPtr<MTL::RenderCommandEncoder>();
        }
      encDraw = NsPtr<MTL::RenderCommandEncoder>(impl->renderCommandEncoder(desc));
      encDraw->retain();
      break;
      }
    case E_Comp: {
      if(encComp!=nullptr)
        return;
      encComp = NsPtr<MTL::ComputeCommandEncoder>(impl->computeCommandEncoder());
      encComp->retain();
      break;
      }
    case E_Blit: {
      if(encBlit!=nullptr)
        return;
      encBlit = NsPtr<MTL::BlitCommandEncoder>(impl->blitCommandEncoder());
      encBlit->retain();
      break;
      }
    }
  implSetDebugLabel();
  }

void MtCommandBuffer::setComputePipeline(AbstractGraphicsApi::CompPipeline &p) {
  setEncoder(E_Comp,nullptr);

  auto& px = reinterpret_cast<MtCompPipeline&>(p);
  encComp->setComputePipelineState(px.impl.get());
  curCompPipeline = &px;
  curLay          = px.lay.handler;
  localSize       = px.localSize;
  maxTotalThreadsPerThreadgroup = uint32_t(px.impl->maxTotalThreadsPerThreadgroup());
  bindings.durty = true;
  pushData.durty = true;
  }

void MtCommandBuffer::setPushData(const void* data, size_t size) {
  pushData.size = uint8_t(size);
  std::memcpy(pushData.data, data, size);

  pushData.durty = true;
  }

void MtCommandBuffer::setBinding(size_t id, AbstractGraphicsApi::Texture* tex, const Sampler& smp, uint32_t mipLevel) {
  bindings.data  [id] = tex;
  bindings.smp   [id] = smp;
  bindings.offset[id] = mipLevel;
  bindings.durty      = true;
  bindings.array      = bindings.array & ~(1u << id);
  }

void MtCommandBuffer::setBinding(size_t id, AbstractGraphicsApi::Buffer* buf, size_t offset) {
  bindings.data  [id] = buf;
  bindings.offset[id] = uint32_t(offset);
  bindings.durty      = true;
  bindings.array      = bindings.array & ~(1u << id);
  }

void MtCommandBuffer::setBinding(size_t id, AbstractGraphicsApi::DescArray* arr) {
  bindings.data[id] = arr;
  bindings.durty    = true;
  bindings.array    = bindings.array | (1u << id);
  }

void MtCommandBuffer::setBinding(size_t id, AbstractGraphicsApi::AccelerationStructure* tlas) {
  bindings.data[id] = tlas;
  bindings.durty    = true;
  bindings.array    = bindings.array & ~(1u << id);
  }

void MtCommandBuffer::setBinding(size_t id, const Sampler& smp) {
  bindings.smp[id] = smp;
  bindings.durty   = true;
  bindings.array   = bindings.array & ~(1u << id);
  }

void MtCommandBuffer::implSetUniforms(const PipelineStage st) {
  if(!bindings.durty)
    return;
  bindings.durty = false;

  auto& lay = curLay->lay;
  auto& mtl = curLay->bind;

  for(size_t i=0; i<lay.size(); ++i) {
    auto& l = lay[i];
    if(l.stage==0)
      continue;
    auto  data   = bindings.data  [l.layout];
    auto  offset = bindings.offset[l.layout];
    auto& smp    = bindings.smp   [l.layout];

    switch(l.cls) {
      case ShaderReflection::Push:
      case ShaderReflection::Count:
        break;
      case ShaderReflection::Ubo:
      case ShaderReflection::SsboR:
      case ShaderReflection::SsboRW: {
        if(l.isArray()) {
          auto arr = reinterpret_cast<MtDescriptorArray2*>(data);
          if(encComp!=nullptr)
            arr->useResource(*encComp, ShaderReflection::Compute);
          else if(encDraw!=nullptr)
            arr->useResource(*encDraw, l.stage);
          setBindless(mtl[i], &arr->data(), arr->size());
          }
        else if(T_LIKELY(data!=nullptr)) {
          auto buf  = reinterpret_cast<MtBuffer*>(data);
          setBuffer(mtl[i], buf->impl.get(), offset);
          } else {
          setBuffer(mtl[i], nullptr, 0);
          }
        break;
        }
      case ShaderReflection::Texture:
      case ShaderReflection::Image: {
        if(l.isArray()) {
          auto arr = reinterpret_cast<MtDescriptorArray2*>(data);
          if(encComp!=nullptr)
            arr->useResource(*encComp, ShaderReflection::Compute);
          else if(encDraw!=nullptr)
            arr->useResource(*encDraw, l.stage);
          setBindless(mtl[i], &arr->data(), arr->size());
          } else {
          auto* sx  = &device.samplers.get(smp);
          auto& tex = reinterpret_cast<MtTexture*>(data)->view(smp.mapping, offset);
          setTexture(mtl[i], &tex, sx);
          }
        break;
        }
      case ShaderReflection::ImgR:
      case ShaderReflection::ImgRW: {
        auto* tex  = reinterpret_cast<MtTexture*>(data);
        auto& view = tex->view(ComponentMapping(), offset);
        setImage(mtl[i], &view, tex->linearMem.get()); //FIXME: mip-mapping for linear ssbo
        break;
        }
      case ShaderReflection::Sampler: {
        auto sx  = &device.samplers.get(smp);
        setTexture(mtl[i], nullptr, sx);
        break;
        }
      case ShaderReflection::Tlas: {
        auto tlas = reinterpret_cast<MtTopAccelerationStructure*>(data);
        if(encComp!=nullptr)
          tlas->useResource(*encComp, ShaderReflection::Compute);
        else if(encDraw!=nullptr)
          tlas->useResource(*encDraw, l.stage);
        setTlas(mtl[i], tlas->impl.get());
        break;
        }
      }
    }
  implSetAlux(st);
  }

void MtCommandBuffer::implSetAlux(const PipelineStage st) {
  const auto& lay              = *curLay;
  const auto  bufferSizeBuffer = lay.bufferSizeBuffer;

  if(bufferSizeBuffer==ShaderReflection::None)
    return;

  uint32_t bufSz[MtShader::MSL_BUFFER_LENGTH_SIZE] = {};

  if(bufferSizeBuffer & ShaderReflection::Task) {
    fillBufferSizeBuffer(bufSz, ShaderReflection::Task);
    encDraw->setObjectBytes(bufSz, sizeof(bufSz), MtShader::MSL_BUFFER_LENGTH);
    }
  if(bufferSizeBuffer & ShaderReflection::Mesh) {
    fillBufferSizeBuffer(bufSz, ShaderReflection::Mesh);
    encDraw->setMeshBytes(bufSz, sizeof(bufSz), MtShader::MSL_BUFFER_LENGTH);
    }
  if(bufferSizeBuffer & ShaderReflection::Vertex) {
    fillBufferSizeBuffer(bufSz, ShaderReflection::Vertex);
    encDraw->setVertexBytes(bufSz, sizeof(bufSz), MtShader::MSL_BUFFER_LENGTH);
    }
  if(bufferSizeBuffer & ShaderReflection::Fragment) {
    fillBufferSizeBuffer(bufSz, ShaderReflection::Fragment);
    encDraw->setFragmentBytes(bufSz, sizeof(bufSz), MtShader::MSL_BUFFER_LENGTH);
    }
  if(bufferSizeBuffer & ShaderReflection::Compute) {
    fillBufferSizeBuffer(bufSz, ShaderReflection::Compute);
    encComp->setBytes(bufSz, sizeof(bufSz), MtShader::MSL_BUFFER_LENGTH);
    }
  }

void MtCommandBuffer::implSetPushData(const PipelineStage st) {
  if(!pushData.durty)
    return;
  pushData.durty = false;

  auto& mtl = curLay->bindPush;
  auto& l   = curLay->pb;

  if(l.stage==0)
    return;

  const uint32_t sz = l.size; // spirv-size, actual size can be bigger
  assert(sz<=pushData.size);

  if(mtl.bindTs!=uint32_t(-1)) {
    encDraw->setObjectBytes(pushData.data, mtl.mslPushSize, mtl.bindTs);
    }
  if(mtl.bindMs!=uint32_t(-1)) {
    encDraw->setMeshBytes(pushData.data, mtl.mslPushSize, mtl.bindMs);
    }
  if(mtl.bindVs!=uint32_t(-1)) {
    encDraw->setVertexBytes(pushData.data, mtl.mslPushSize, mtl.bindVs);
    }
  if(mtl.bindFs!=uint32_t(-1)) {
    encDraw->setFragmentBytes(pushData.data, mtl.mslPushSize, mtl.bindFs);
    }
  if(mtl.bindCs!=uint32_t(-1)) {
    encComp->setBytes(pushData.data, mtl.mslPushSize, mtl.bindCs);
    }
  }

void MtCommandBuffer::fillBufferSizeBuffer(uint32_t* ret, ShaderReflection::Stage stage) {
  const auto& lay = *curLay;
  auto& lx  = lay.lay;
  auto& mtl = lay.bind;
  for(size_t i=0; i<lx.size(); ++i) {
    if(lx[i].cls!=ShaderReflection::SsboR && lx[i].cls!=ShaderReflection::SsboRW)
      continue;
    if(bindings.array & (1u << lx[i].layout))
      continue;

    uint32_t at = uint32_t(-1);
    switch(stage) {
      case ShaderReflection::None:
      case ShaderReflection::Control:
      case ShaderReflection::Evaluate:
      case ShaderReflection::Geometry:
        break;
      case ShaderReflection::Task:
        at = mtl[i].bindTs;
        break;
      case ShaderReflection::Mesh:
        at = mtl[i].bindMs;
        break;
      case ShaderReflection::Vertex:
        at = mtl[i].bindVs;
        break;
      case ShaderReflection::Fragment:
        at = mtl[i].bindFs;
        break;
      case ShaderReflection::Compute:
        at = mtl[i].bindCs;
        break;
      }
    if(at==uint32_t(-1))
      continue;

    auto* data = reinterpret_cast<const MtBuffer*>(bindings.data[i]);
    ret[at] = data!=nullptr ? uint32_t(data->size - bindings.offset[i]) : 0;
    }
  }

void MtCommandBuffer::setViewport(const Rect &r) {
  MTL::Viewport v = {};
  v.originX = r.x;
  v.originY = r.y;
  v.width   = r.w;
  v.height  = r.h;
  v.znear   = 0;
  v.zfar    = 1;

  encDraw->setViewport(v);
  }

void MtCommandBuffer::setScissor(const Rect& r) {
  int x0 = std::max(0,r.x), y0 = std::max(0,r.y);
  int w  = (r.x+r.w)-x0, h = (r.y+r.h)-y0;

  MTL::ScissorRect v = {};
  v.x      = x0;
  v.y      = y0;
  v.width  = w;
  v.height = h;
  encDraw->setScissorRect(v);
  }

void MtCommandBuffer::setDebugMarker(std::string_view tag) {
  activeLabel = tag;

  MTL::CommandEncoder* enc = nullptr;
  if(encDraw!=nullptr)
    enc = encDraw.get();
  if(encComp!=nullptr)
    enc = encComp.get();
  if(encBlit!=nullptr)
    enc = encBlit.get();

  if(enc==nullptr) {
    return;
    }

  if(isDbgRegion) {
    enc->popDebugGroup();
    isDbgRegion = false;
    }

  if(!tag.empty()) {
    auto pool = NsPtr<NS::AutoreleasePool>::init();
    char buf[128] = {};
    std::snprintf(buf, sizeof(buf), "%.*s", int(tag.size()), tag.data());
    auto str = NsPtr<NS::String>(NS::String::string(buf,NS::UTF8StringEncoding));
    str->retain();

    enc->pushDebugGroup(str.get());
    isDbgRegion = true;
    }
  }

void MtCommandBuffer::implSetDebugLabel() {
  if(activeLabel.empty())
    return;

  MTL::CommandEncoder* enc = nullptr;
  if(encDraw!=nullptr)
    enc = encDraw.get();
  if(encComp!=nullptr)
    enc = encComp.get();
  if(encBlit!=nullptr)
    enc = encBlit.get();

  if(enc==nullptr) {
    return;
    }

  auto pool = NsPtr<NS::AutoreleasePool>::init();
  char buf[128] = {};
  std::snprintf(buf, sizeof(buf), "%s", activeLabel.c_str());
  if(enc->label()!=nullptr) {
    std::snprintf(buf, sizeof(buf), "%s & %s", enc->label()->cString(NS::UTF8StringEncoding), activeLabel.c_str());
    }
  auto str = NsPtr<NS::String>(NS::String::string(buf,NS::UTF8StringEncoding));
  str->retain();
  enc->setLabel(str.get());

  if(isDbgRegion)
    enc->popDebugGroup();
  enc->pushDebugGroup(str.get());
  isDbgRegion = true;
  }

void MtCommandBuffer::draw(const AbstractGraphicsApi::Buffer* ivbo, size_t stride, size_t offset, size_t vertexCount,
                           size_t firstInstance, size_t instanceCount) {
  auto* vbo = reinterpret_cast<const MtBuffer*>(ivbo);

  if(T_UNLIKELY(stride!=vboStride)) {
    auto& px   = *curDrawPipeline;
    auto& inst = px.inst(curFbo,px.defaultStride);
    encDraw->setRenderPipelineState(&inst);
    }

  if(T_LIKELY(vbo!=nullptr)) {
    encDraw->setVertexBuffer(vbo->impl.get(),0,curVboId);
    }

  implSetUniforms(S_Graphics);
  implSetPushData(S_Graphics);
  encDraw->drawPrimitives(topology,offset,vertexCount,instanceCount,firstInstance);
  }

void MtCommandBuffer::drawIndexed(const AbstractGraphicsApi::Buffer* ivbo, size_t stride, size_t voffset,
                                  const AbstractGraphicsApi::Buffer& iibo, Detail::IndexClass icls, size_t ioffset, size_t isize,
                                  size_t firstInstance, size_t instanceCount) {
  auto*    vbo     = reinterpret_cast<const MtBuffer*>(ivbo);
  auto&    ibo     = reinterpret_cast<const MtBuffer&>(iibo);
  auto     iboType = nativeFormat(icls);
  uint32_t mul     = sizeofIndex(icls);

  if(T_UNLIKELY(stride!=vboStride)) {
    auto& px   = *curDrawPipeline;
    auto& inst = px.inst(curFbo,px.defaultStride);
    encDraw->setRenderPipelineState(&inst);
    }

  if(T_LIKELY(vbo!=nullptr)) {
    encDraw->setVertexBuffer(vbo->impl.get(),0,curVboId);
    }
  implSetUniforms(S_Graphics);
  implSetPushData(S_Graphics);
  encDraw->drawIndexedPrimitives(topology,isize,iboType,ibo.impl.get(),
                                 ioffset*mul,instanceCount,voffset,firstInstance);
  }

void MtCommandBuffer::drawIndirect(const AbstractGraphicsApi::Buffer& indirect, size_t offset) {
  implSetUniforms(S_Graphics);
  implSetPushData(S_Graphics);
  auto& ind = reinterpret_cast<const MtBuffer&>(indirect);
  encDraw->drawPrimitives(topology, ind.impl.get(), offset);
  }

void MtCommandBuffer::dispatchMesh(size_t x, size_t y, size_t z) {
  implSetUniforms(S_Graphics);
  implSetPushData(S_Graphics);
  // NOTE: for taskless pipeline `localSize` must be zero
  encDraw->drawMeshThreadgroups(MTL::Size(x,y,z), localSize, localSizeMesh);
  }

void MtCommandBuffer::dispatchMeshIndirect(const AbstractGraphicsApi::Buffer& indirect, size_t offset) {
  implSetUniforms(S_Graphics);
  implSetPushData(S_Graphics);
  auto& ind = reinterpret_cast<const MtBuffer&>(indirect);
  encDraw->drawMeshThreadgroups(ind.impl.get(), offset, localSize, localSizeMesh);
  }

void MtCommandBuffer::dispatch(size_t x, size_t y, size_t z) {
  implSetUniforms(S_Compute);
  implSetPushData(S_Compute);
  encComp->dispatchThreadgroups(MTL::Size(x,y,z), localSize);
  }

void MtCommandBuffer::dispatchIndirect(const AbstractGraphicsApi::Buffer& indirect, size_t offset) {
  implSetUniforms(S_Compute);
  auto& ind = reinterpret_cast<const MtBuffer&>(indirect);
  encComp->dispatchThreadgroups(ind.impl.get(), offset, localSize);
  }

void MtCommandBuffer::setBuffer(const MtPipelineLay::MTLBind& mtl, MTL::Buffer* buf, size_t offset) {
  if(mtl.bindTs!=uint32_t(-1)) {
    encDraw->setObjectBuffer(buf,offset,mtl.bindTs);
    }
  if(mtl.bindMs!=uint32_t(-1)) {
    encDraw->setMeshBuffer(buf,offset,mtl.bindMs);
    }
  if(mtl.bindVs!=uint32_t(-1)) {
    encDraw->setVertexBuffer(buf,offset,mtl.bindVs);
    }
  if(mtl.bindFs!=uint32_t(-1)) {
    encDraw->setFragmentBuffer(buf,offset,mtl.bindFs);
    }
  if(mtl.bindCs!=uint32_t(-1)) {
    encComp->setBuffer(buf,offset,mtl.bindCs);
    }
  }

void MtCommandBuffer::setTexture(const MtPipelineLay::MTLBind& mtl, MTL::Texture* tex, MTL::SamplerState* ss) {
  if(mtl.bindTs!=uint32_t(-1))
    encDraw->setObjectTexture(tex,mtl.bindTs);
  if(mtl.bindTsSmp!=uint32_t(-1))
    encDraw->setObjectSamplerState(ss,mtl.bindTsSmp);

  if(mtl.bindMs!=uint32_t(-1))
    encDraw->setMeshTexture(tex,mtl.bindMs);
  if(mtl.bindMsSmp!=uint32_t(-1))
    encDraw->setMeshSamplerState(ss,mtl.bindMsSmp);

  if(mtl.bindVs!=uint32_t(-1))
    encDraw->setVertexTexture(tex,mtl.bindVs);
  if(mtl.bindVsSmp!=uint32_t(-1))
    encDraw->setVertexSamplerState(ss,mtl.bindVsSmp);

  if(mtl.bindFs!=uint32_t(-1))
    encDraw->setFragmentTexture(tex,mtl.bindFs);
  if(mtl.bindFsSmp!=uint32_t(-1))
    encDraw->setFragmentSamplerState(ss,mtl.bindFsSmp);

  if(mtl.bindCs!=uint32_t(-1))
    encComp->setTexture(tex,mtl.bindCs);
  if(mtl.bindCsSmp!=uint32_t(-1))
    encComp->setSamplerState(ss,mtl.bindCsSmp);
  }

void MtCommandBuffer::setImage(const MtPipelineLay::MTLBind& mtl, MTL::Texture* tex, MTL::Buffer* atom) {
  if(mtl.bindTs!=uint32_t(-1))
    encDraw->setObjectTexture(tex,mtl.bindTs);
  if(mtl.bindTsSmp!=uint32_t(-1))
    encDraw->setObjectBuffer(atom,0,mtl.bindTsSmp);

  if(mtl.bindMs!=uint32_t(-1))
    encDraw->setMeshTexture(tex,mtl.bindMs);
  if(mtl.bindMsSmp!=uint32_t(-1))
    encDraw->setMeshBuffer(atom,0,mtl.bindMsSmp);

  if(mtl.bindVs!=uint32_t(-1))
    encDraw->setVertexTexture(tex,mtl.bindVs);
  if(mtl.bindVsSmp!=uint32_t(-1))
    encDraw->setVertexBuffer(atom,0,mtl.bindVsSmp);

  if(mtl.bindFs!=uint32_t(-1))
    encDraw->setFragmentTexture(tex,mtl.bindFs);
  if(mtl.bindFsSmp!=uint32_t(-1))
    encDraw->setFragmentBuffer(atom,0,mtl.bindFsSmp);

  if(mtl.bindCs!=uint32_t(-1))
    encComp->setTexture(tex,mtl.bindCs);
  if(mtl.bindCsSmp!=uint32_t(-1))
    encComp->setBuffer(atom,0,mtl.bindCsSmp);
  }

void MtCommandBuffer::setTlas(const MtPipelineLay::MTLBind& mtl, MTL::AccelerationStructure* as) {
  if(mtl.bindTs!=uint32_t(-1)) {
    // not suported?
    // encDraw->setObjectAccelerationStructure(as,mtl.bindTs);
    }
  if(mtl.bindMs!=uint32_t(-1)) {
    // not suported?
    // encDraw->setMeshAccelerationStructure(as,mtl.bindMs);
    }
  if(mtl.bindVs!=uint32_t(-1)) {
    encDraw->setVertexAccelerationStructure(as,mtl.bindVs);
    }
  if(mtl.bindFs!=uint32_t(-1)) {
    encDraw->setFragmentAccelerationStructure(as,mtl.bindFs);
    }
  if(mtl.bindCs!=uint32_t(-1)) {
    encComp->setAccelerationStructure(as,mtl.bindCs);
    }
  }

void MtCommandBuffer::setBindless(const MtPipelineLay::MTLBind& mtl, MTL::Buffer* buf, size_t numRes) {
  if(mtl.bindTs!=uint32_t(-1)) {
    encDraw->setObjectBuffer(buf,0,mtl.bindTs);
    }
  if(mtl.bindMs!=uint32_t(-1)) {
    encDraw->setMeshBuffer(buf,0,mtl.bindMs);
    }
  if(mtl.bindVs!=uint32_t(-1)) {
    encDraw->setVertexBuffer(buf,0,mtl.bindVs);
    }
  if(mtl.bindFs!=uint32_t(-1)) {
    encDraw->setFragmentBuffer(buf,0,mtl.bindFs);
    }
  if(mtl.bindCs!=uint32_t(-1)) {
    encComp->setBuffer(buf,0,mtl.bindCs);
    }

  // samplers
  uint32_t shift = uint32_t(numRes * sizeof(MTL::ResourceID));
  shift  = ((shift+16-1)/16)*16;

  if(mtl.bindTsSmp!=uint32_t(-1)) {
    encDraw->setObjectBuffer(buf,shift,mtl.bindTsSmp);
    }
  if(mtl.bindMsSmp!=uint32_t(-1)) {
    encDraw->setMeshBuffer(buf,shift,mtl.bindMsSmp);
    }
  if(mtl.bindVsSmp!=uint32_t(-1)) {
    encDraw->setVertexBuffer(buf,shift,mtl.bindVsSmp);
    }
  if(mtl.bindFsSmp!=uint32_t(-1)) {
    encDraw->setFragmentBuffer(buf,shift,mtl.bindFsSmp);
    }
  if(mtl.bindCsSmp!=uint32_t(-1)) {
    encComp->setBuffer(buf,shift,mtl.bindCsSmp);
    }
  }

void MtCommandBuffer::generateMipmap(AbstractGraphicsApi::Texture &image,
                                     uint32_t /*texWidth*/, uint32_t /*texHeight*/,
                                     uint32_t /*mipLevels*/) {
  setEncoder(E_Blit,nullptr);

  auto& t = reinterpret_cast<MtTexture&>(image);
  encBlit->generateMipmaps(t.impl.get());
  }

void MtCommandBuffer::setPipeline(AbstractGraphicsApi::Pipeline &p) {
  auto& px   = reinterpret_cast<MtPipeline&>(p);
  auto& inst = px.inst(curFbo,px.defaultStride);

  if(curFbo.depthFormat!=MTL::PixelFormatInvalid)
    encDraw->setDepthStencilState(px.depthStZ.get()); else
    encDraw->setDepthStencilState(px.depthStNoZ.get());
  encDraw->setRenderPipelineState(&inst);
  encDraw->setCullMode(px.cullMode);

  topology        = px.topology;
  curDrawPipeline = &px;
  vboStride       = px.defaultStride;
  curLay          = px.lay.handler;
  curVboId        = px.lay.handler->vboIndex;
  localSize       = px.localSize;
  localSizeMesh   = px.localSizeMesh;

  bindings.durty  = true;
  pushData.durty  = true;
  }

void MtCommandBuffer::copy(AbstractGraphicsApi::Buffer& dest, size_t offset,
                           AbstractGraphicsApi::Texture& src, uint32_t width, uint32_t height, uint32_t mip) {
  setEncoder(E_Blit,nullptr);

  auto& s = reinterpret_cast<MtTexture&>(src);
  auto& d = reinterpret_cast<MtBuffer&> (dest);
  const uint32_t bpp = s.bitCount()/8;

  encBlit->copyFromTexture(s.impl.get(), 0,mip,
                           MTL::Origin(0,0,0),MTL::Size(width,height,1),
                           d.impl.get(),
                           offset, bpp*width,bpp*width*height);
  }

#endif
