#if defined(TEMPEST_BUILD_VULKAN)

#include "vcommandbuffer.h"

#include <Tempest/Attachment>

#include "vdevice.h"
#include "vcommandpool.h"
#include "vpipeline.h"
#include "vbuffer.h"
#include "vdescriptorarray.h"
#include "vswapchain.h"
#include "vtexture.h"
#include "vframebuffermap.h"
#include "vaccelerationstructure.h"

using namespace Tempest;
using namespace Tempest::Detail;

static VkAttachmentLoadOp mkLoadOp(const AccessOp op) {
  switch (op) {
    case AccessOp::Discard:
      return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    case AccessOp::Preserve:
      return VK_ATTACHMENT_LOAD_OP_LOAD;
    case AccessOp::Clear:
      return VK_ATTACHMENT_LOAD_OP_CLEAR;
    case AccessOp::Readonly:
      return VK_ATTACHMENT_LOAD_OP_LOAD;
    }
  return VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  }

static VkAttachmentStoreOp mkStoreOp(const AccessOp op) {
  switch (op) {
    case AccessOp::Discard:
      return VK_ATTACHMENT_STORE_OP_DONT_CARE;
    case AccessOp::Preserve:
      return VK_ATTACHMENT_STORE_OP_STORE;
    case AccessOp::Clear:
      return VK_ATTACHMENT_STORE_OP_STORE;
    case AccessOp::Readonly:
      return VK_ATTACHMENT_STORE_OP_NONE; // dynamic-rendering guaranties store_op_none
    }
  return VK_ATTACHMENT_STORE_OP_DONT_CARE;
  }

static void toStage(VDevice& dev, VkPipelineStageFlags& stage, VkAccessFlags& access, SyncStage rs, bool isSrc) {
  uint32_t ret = 0;
  uint32_t acc = 0;
  if((rs&SyncStage::TransferSrc)==SyncStage::TransferSrc) {
    ret |= VK_PIPELINE_STAGE_TRANSFER_BIT;
    acc |= VK_ACCESS_TRANSFER_READ_BIT;
    }
  if((rs&SyncStage::TransferDst)==SyncStage::TransferDst) {
    ret |= VK_PIPELINE_STAGE_TRANSFER_BIT;
    acc |= VK_ACCESS_TRANSFER_WRITE_BIT;
    }
  if((rs&SyncStage::TransferHost)==SyncStage::TransferHost) {
    ret |= VK_PIPELINE_STAGE_HOST_BIT;
    acc |= VK_ACCESS_HOST_READ_BIT;
    }

  if((rs&SyncStage::Indirect)==SyncStage::Indirect) {
    ret |= VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT;
    acc |= VK_ACCESS_INDIRECT_COMMAND_READ_BIT;
    }

  if((rs&SyncStage::ComputeRead)==SyncStage::ComputeRead) {
    ret |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    acc |= VK_ACCESS_SHADER_READ_BIT;
    acc |= VK_ACCESS_UNIFORM_READ_BIT;
    }
  if((rs&SyncStage::ComputeWrite)==SyncStage::ComputeWrite) {
    ret |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    acc |= VK_ACCESS_SHADER_WRITE_BIT;
    }

  if((rs&SyncStage::GraphicsRead)==SyncStage::GraphicsRead) {
    ret |= VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;
    acc |= VK_ACCESS_SHADER_READ_BIT;
    acc |= VK_ACCESS_UNIFORM_READ_BIT;
    acc |= VK_ACCESS_INDEX_READ_BIT;
    acc |= VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
    }
  if((rs&SyncStage::GraphicsWrite)==SyncStage::GraphicsWrite) {
    ret |= VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;
    acc |= VK_ACCESS_SHADER_WRITE_BIT;
    // acc |= VK_ACCESS_INPUT_ATTACHMENT_READ_BIT; // not a use-case
    }

  if((rs&SyncStage::GraphicsDraw)==SyncStage::GraphicsDraw) {
    ret |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    acc |= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    }
  if((rs&SyncStage::GraphicsDepth)==SyncStage::GraphicsDepth) {
    ret |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    acc |= VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    }

  if(dev.props.raytracing.rayQuery) {
    if((rs&SyncStage::RtAsRead)==SyncStage::RtAsRead) {
      ret |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
      ret |= VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;
      ret |= VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR;
      acc |= VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
      }
    if((rs&SyncStage::RtAsWrite)==SyncStage::RtAsWrite) {
      ret |= VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR;
      acc |= VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
      }
    }

  if(isSrc && ret==0) {
    // wait for nothing: asset uploading case
    ret = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    acc = VK_ACCESS_NONE_KHR;
    }
  stage  = VkPipelineStageFlags(ret);
  access = VkAccessFlags(acc);
  }

static VkImageLayout toLayout(ResourceLayout rs, const VTexture* texture) {
  if(rs==ResourceLayout::None)
    return VK_IMAGE_LAYOUT_UNDEFINED;

  if(rs==ResourceLayout::TransferSrc)
    return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
  if(rs==ResourceLayout::TransferDst)
    return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  if(rs==ResourceLayout::Default) {
    if(texture==nullptr)
      return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    if(nativeIsDepthFormat(texture->format))
      return VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
    if(texture->isStorageImage)
      return VK_IMAGE_LAYOUT_GENERAL;
    return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }

  if(rs==ResourceLayout::ColorAttach)
    return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  if(rs==ResourceLayout::DepthReadOnly)
    return VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
  if(rs==ResourceLayout::DepthAttach)
    return VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;

  return VK_IMAGE_LAYOUT_GENERAL;
  }

static VkImage toVkResource(const AbstractGraphicsApi::BarrierDesc& b) {
  if(b.texture!=nullptr) {
    auto& t = *reinterpret_cast<const VTexture*>(b.texture);
    return t.impl;
    }

  auto& s = *reinterpret_cast<const VSwapchain*>(b.swapchain);
  return s.images[b.swId];
  }


VCommandBuffer::VCommandBuffer(VDevice& device, VkCommandPoolCreateFlags flags)
  :device(device), pool(device,flags), pushDescriptors(device) {
  }

VCommandBuffer::~VCommandBuffer() {
  if(impl!=nullptr) {
    vkFreeCommandBuffers(device.device.impl,pool.impl,1,&impl);
    }

  if(chunks.size()==0)
    return;

  SmallArray<VkCommandBuffer,MaxCmdChunks> flat(chunks.size());
  auto node = chunks.begin();
  for(size_t i=0; i<chunks.size(); ++i) {
    flat[i] = node->val[i%chunks.chunkSize].impl;
    if(i+1==chunks.chunkSize)
      node = node->next;
    }
  vkFreeCommandBuffers(device.device.impl,pool.impl,uint32_t(chunks.size()),flat.get());
  }

void VCommandBuffer::reset() {
  vkAssert(vkResetCommandPool(device.device.impl,pool.impl,0));

  SmallArray<VkCommandBuffer,MaxCmdChunks> flat(chunks.size());
  auto node = chunks.begin();
  if(chunks.size()>0) {
    impl = node->val[0].impl;
    vkResetCommandBuffer(impl,VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);
    }
  for(size_t i=1; i<chunks.size(); ++i) {
    auto cmd = node->val[i%chunks.chunkSize].impl;
    vkFreeCommandBuffers(device.device.impl,pool.impl,1,&cmd);
    if(i+1==chunks.chunkSize)
      node = node->next;
    }
  chunks.clear();

  swapchainSync.reserve(swapchainSync.size());
  swapchainSync.clear();

  bindings = Bindings();
  pushDescriptors.reset();
  }

void VCommandBuffer::begin(SyncHint hint) {
  state  = Idle;
  curVbo = VK_NULL_HANDLE;
  pushData.size  = 0;
  pushData.durty = true;
  if(chunks.size()>0)
    reset();

  if(hint==Detail::SyncHint::NoPendingReads)
    resState.clearReaders();

  if(impl==nullptr) {
    newChunk();
    } else {
    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags            = 0;
    beginInfo.pInheritanceInfo = nullptr;
    vkAssert(vkBeginCommandBuffer(impl,&beginInfo));
    }
  }

void VCommandBuffer::begin() {
  begin(SyncHint::None);
  }

void VCommandBuffer::end() {
  if(isDbgRegion) {
    device.vkCmdDebugMarkerEnd(impl);
    isDbgRegion = false;
    }
  swapchainSync.reserve(swapchainSync.size());
  resState.finalize(*this);
  state = NoRecording;

  pushChunk();
  }

bool VCommandBuffer::isRecording() const {
  return state!=NoRecording;
  }

void VCommandBuffer::beginRendering(const AttachmentDesc* desc, size_t descSize,
                                    uint32_t width, uint32_t height,
                                    const TextureFormat* frm,
                                    AbstractGraphicsApi::Texture** att,
                                    AbstractGraphicsApi::Swapchain** sw, const uint32_t* imgId) {
  for(size_t i=0; i<descSize; ++i) {
    if(sw[i]!=nullptr)
      addDependency(*reinterpret_cast<VSwapchain*>(sw[i]),imgId[i]);
    }

  resState.joinWriters(PipelineStage::S_Indirect);
  resState.joinWriters(PipelineStage::S_Graphics);
  // resState.flush(*this); //debug

  for(size_t i=0; i<descSize; ++i) {
    if(desc[i].load==AccessOp::Readonly)
      continue;
    NonUniqResId nonUniqId = NonUniqResId::I_None;
    if(sw[i] != nullptr) {
      //auto& t = *reinterpret_cast<VSwapchain*>(sw[i]);
      nonUniqId = NonUniqResId(0x1); //FIXME: track swapchain for real
      }
    else {
      auto& t = *reinterpret_cast<VTexture*>(att[i]);
      nonUniqId = t.nonUniqId;
      }
    resState.onDrawUsage(nonUniqId, desc[i].load);
    }
  resState.setRenderpass(*this,desc,descSize,frm,att,sw,imgId);

  bindings.read  = NonUniqResId::I_None;
  bindings.write = NonUniqResId::I_None;
  bindings.host  = false;

  if(state!=Idle) {
    newChunk();
    }

  {
    VkRenderingAttachmentInfoKHR colorAtt[MaxFramebufferAttachments] = {};
    VkRenderingAttachmentInfoKHR depthAtt = {};

    VkRenderingInfoKHR info = {};
    info.sType                = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR;
    info.flags                = 0;
    info.renderArea.offset    = {0, 0};
    info.renderArea.extent    = {width,height};
    info.layerCount           = 1;
    info.viewMask             = 0;
    info.colorAttachmentCount = 0;
    info.pColorAttachments    = colorAtt;

    passDyn.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
    passDyn.pNext                   = nullptr;
    passDyn.viewMask                = info.viewMask;
    passDyn.colorAttachmentCount    = 0;
    passDyn.pColorAttachmentFormats = passDyn.colorFrm;
    passDyn.depthAttachmentFormat   = VK_FORMAT_UNDEFINED;

    VkImageView imageView   = VK_NULL_HANDLE;
    VkFormat    imageFormat = VK_FORMAT_UNDEFINED;
    for(size_t i=0; i<descSize; ++i) {
      if(sw[i] != nullptr) {
        auto& t = *reinterpret_cast<VSwapchain*>(sw[i]);
        imageView   = t.views[imgId[i]];
        imageFormat = t.format();
        }
      else {
        auto &t = *reinterpret_cast<VTexture*>(att[i]);
        imageView   = t.imgView;
        imageFormat = t.format;
        }

      auto& att = isDepthFormat(frm[i]) ? depthAtt : colorAtt[info.colorAttachmentCount];
      att.sType     = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
      att.imageView = imageView;

      if(isDepthFormat(frm[i])) {
        if(desc[i].load==AccessOp::Readonly)
          att.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL; else
          att.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        info.pDepthAttachment = &att;
        passDyn.depthAttachmentFormat = imageFormat;
        auto& clr = att.clearValue;
        clr.depthStencil.depth = desc[i].clear.x;
        } else {
        passDyn.colorFrm[info.colorAttachmentCount] = imageFormat;
        ++info.colorAttachmentCount;

        att.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        auto& clr = att.clearValue;
        clr.color.float32[0] = desc[i].clear.x;
        clr.color.float32[1] = desc[i].clear.y;
        clr.color.float32[2] = desc[i].clear.z;
        clr.color.float32[3] = desc[i].clear.w;
        }

      att.resolveMode        = VK_RESOLVE_MODE_NONE;
      att.resolveImageView   = VK_NULL_HANDLE;
      att.resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
      att.loadOp             = mkLoadOp(desc[i].load);
      att.storeOp            = mkStoreOp(desc[i].store);
      }
    passDyn.colorAttachmentCount = info.colorAttachmentCount;
    vkCmdBeginRenderingKHR(impl,&info);
  }
  state = RenderPass;

  // setup dynamic state
  // https://www.khronos.org/registry/vulkan/specs/1.1-extensions/html/vkspec.html#pipelines-dynamic-state
  setViewport(Rect(0,0,int32_t(width),int32_t(height)));
  setScissor (Rect(0,0,int32_t(width),int32_t(height)));
  }

void VCommandBuffer::endRendering() {
  vkCmdEndRenderingKHR(impl);

  resState.onUavUsage(bindings.read, bindings.write, PipelineStage::S_Graphics);
  state = PostRenderPass;
  }

void VCommandBuffer::setPipeline(AbstractGraphicsApi::Pipeline& p) {
  VPipeline& px   = reinterpret_cast<VPipeline&>(p);

  bindings.durty  = true;
  curDrawPipeline = &px;
  vboStride       = px.defaultStride;
  pipelineLayout  = VK_NULL_HANDLE; // clear until draw
  }

void VCommandBuffer::setComputePipeline(AbstractGraphicsApi::CompPipeline& p) {
  state = Compute;
  auto& px = reinterpret_cast<VCompPipeline&>(p);

  bindings.durty  = true;
  curCompPipeline = &px;
  pipelineLayout  = VK_NULL_HANDLE; // clear until dispatch
  }

void VCommandBuffer::dispatch(size_t x, size_t y, size_t z) {
  implSetUniforms(PipelineStage::S_Compute);
  implSetPushData(PipelineStage::S_Compute);
  resState.onUavUsage(bindings.read, bindings.write, PipelineStage::S_Compute, bindings.host);
  resState.flush(*this);
  vkCmdDispatch(impl,uint32_t(x),uint32_t(y),uint32_t(z));
  }

void VCommandBuffer::dispatchIndirect(const AbstractGraphicsApi::Buffer& indirect, size_t offset) {
  const VBuffer& ind = reinterpret_cast<const VBuffer&>(indirect);

  implSetUniforms(PipelineStage::S_Compute);
  implSetPushData(PipelineStage::S_Compute);
  resState.onUavUsage(bindings.read, bindings.write, PipelineStage::S_Compute, bindings.host);
  // block future writers
  resState.onUavUsage(ind.nonUniqId, NonUniqResId::I_None, PipelineStage::S_Indirect);
  resState.flush(*this);

  vkCmdDispatchIndirect(impl, ind.impl, VkDeviceSize(offset));
  }

void VCommandBuffer::setPushData(const void* data, size_t size) {
  pushData.size = uint8_t(size);
  std::memcpy(pushData.data, data, size);

  pushData.durty = true;
  }

void VCommandBuffer::handleSync(const ShaderReflection::LayoutDesc& lay, const ShaderReflection::SyncDesc& sync, PipelineStage st) {
  if(st!=PipelineStage::S_Graphics) {
    bindings.read  = NonUniqResId::I_None;
    bindings.write = NonUniqResId::I_None;
    bindings.host  = false;
    }

  for(size_t i=0; i<MaxBindings; ++i) {
    NonUniqResId nonUniqId = NonUniqResId::I_None;
    auto         data      = bindings.data[i];
    if(data==nullptr)
      continue;
    switch(lay.bindings[i]) {
      case ShaderReflection::Texture:
      case ShaderReflection::Image:
      case ShaderReflection::ImgR:
      case ShaderReflection::ImgRW: {
        if((bindings.array & (1u << i))!=0) {
          nonUniqId = reinterpret_cast<VDescriptorArray*>(data)->nonUniqId;
          } else {
          nonUniqId = reinterpret_cast<VTexture*>(data)->nonUniqId;
          }
        break;
        }
      case ShaderReflection::Ubo:
      case ShaderReflection::SsboR:
      case ShaderReflection::SsboRW: {
        if((bindings.array & (1u << i))!=0) {
          nonUniqId = reinterpret_cast<VDescriptorArray*>(data)->nonUniqId;
          } else {
          auto buf = reinterpret_cast<VBuffer*>(data);
          nonUniqId = buf->nonUniqId;
          if(lay.bindings[i]==ShaderReflection::SsboRW)
            bindings.host |= buf->isHostVisible();
          }
        break;
        }
      case ShaderReflection::Tlas: {
        //NOTE: Tlas tracking is not really implemented
        assert((bindings.array & (1u << i))==0);
        nonUniqId = reinterpret_cast<VAccelerationStructure*>(data)->data.nonUniqId;
        break;
        }
      case ShaderReflection::Sampler:
      case ShaderReflection::Push:
      case ShaderReflection::Count:
        break;
      }

    if(sync.read & (1u<<i))
      bindings.read |= nonUniqId;
    if(sync.write & (1u<<i))
      bindings.write |= nonUniqId;
    }
  }

void VCommandBuffer::implSetUniforms(const PipelineStage st) {
  if(!bindings.durty)
    return;
  bindings.durty = false;

  using PushBlock  = ShaderReflection::PushBlock;
  using LayoutDesc = ShaderReflection::LayoutDesc;
  using SyncDesc   = ShaderReflection::SyncDesc;

  const LayoutDesc*   lay       = nullptr;
  const SyncDesc*     sync      = nullptr;
  const PushBlock*    pb        = nullptr;

  VkPipelineLayout    pLay      = VK_NULL_HANDLE;
  VkPipelineBindPoint bindPoint = VK_PIPELINE_BIND_POINT_MAX_ENUM;
  switch(st) {
    case PipelineStage::S_Graphics:
      lay       = &curDrawPipeline->layout;
      sync      = &curDrawPipeline->sync;
      pb        = &curDrawPipeline->pb;
      pLay      = curDrawPipeline->pipelineLayout;
      bindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
      break;
    case PipelineStage::S_Compute:
      lay       = &curCompPipeline->layout;
      sync      = &curCompPipeline->sync;
      pb        = &curCompPipeline->pb;
      pLay      = curCompPipeline->pipelineLayout;
      bindPoint = VK_PIPELINE_BIND_POINT_COMPUTE;
      break;
    default:
      break;
    }

  handleSync(*lay, *sync, st);

  if(lay->isUpdateAfterBind()) {
    auto dset = device.bindless.inst(*pb, *lay, bindings);
    pLay = dset.pLay;
    vkCmdBindDescriptorSets(impl, bindPoint,
                            pLay, 0, 1,
                            &dset.set, 0, nullptr);
    } else {
    auto dset = pushDescriptors.push(*pb, *lay, bindings);
    vkCmdBindDescriptorSets(impl, bindPoint,
                            pLay, 0, 1,
                            &dset, 0, nullptr);
    }

  if(pLay!=pipelineLayout && st==PipelineStage::S_Graphics) {
    pipelineLayout = pLay;
    auto& pso  = *curDrawPipeline;
    auto  rp   = (passRp!=nullptr ? passRp->pass : VK_NULL_HANDLE);
    auto  inst = pso.instance(passDyn, rp, pipelineLayout, vboStride);
    vkCmdBindPipeline(impl, bindPoint, inst);
    pushData.durty = true;
    }
  else if(pLay!=pipelineLayout && st==PipelineStage::S_Compute) {
    pipelineLayout = pLay;
    auto& pso  = *curCompPipeline;
    auto  inst = pso.instance(pipelineLayout);
    vkCmdBindPipeline(impl, bindPoint, inst);
    pushData.durty = true;
    }
  }

void VCommandBuffer::implSetPushData(const PipelineStage st) {
  if(!pushData.durty)
    return;
  pushData.durty = false;

  const ShaderReflection::PushBlock* pb = nullptr;
  switch(st) {
    case PipelineStage::S_Graphics:
      pb = &curDrawPipeline->pb;
      break;
    case PipelineStage::S_Compute:
      pb = &curCompPipeline->pb;
      break;
    default:
      break;
    }

  if(pb->size==0)
    return;

  assert(pb->size<=pushData.size);
  auto stages = nativeFormat(pb->stage);
  vkCmdPushConstants(impl, pipelineLayout, stages, 0, uint32_t(pb->size), pushData.data);
  }

void VCommandBuffer::setBinding(size_t id, AbstractGraphicsApi::Texture *tex, uint32_t mipLevel, const ComponentMapping& map, const Sampler &smp) {
  bindings.data  [id] = tex;
  bindings.smp   [id] = smp;
  bindings.map   [id] = map;
  bindings.offset[id] = mipLevel;
  bindings.durty      = true;
  bindings.array      = bindings.array & ~(1u << id);
  }

void VCommandBuffer::setBinding(size_t id, AbstractGraphicsApi::Buffer *buf, size_t offset) {
  bindings.data  [id] = buf;
  bindings.offset[id] = uint32_t(offset);
  bindings.durty      = true;
  bindings.array      = bindings.array & ~(1u << id);
  }

void VCommandBuffer::setBinding(size_t id, AbstractGraphicsApi::DescArray *arr) {
  bindings.data[id] = arr;
  bindings.durty    = true;
  bindings.array    = bindings.array | (1u << id);
  }

void VCommandBuffer::setBinding(size_t id, AbstractGraphicsApi::AccelerationStructure* tlas) {
  bindings.data[id] = tlas;
  bindings.durty    = true;
  bindings.array    = bindings.array & ~(1u << id);
  }

void VCommandBuffer::setBinding(size_t id, const Sampler &smp) {
  bindings.smp[id] = smp;
  bindings.durty   = true;
  bindings.array   = bindings.array & ~(1u << id);
  }

void VCommandBuffer::draw(const AbstractGraphicsApi::Buffer* ivbo, size_t stride, size_t voffset, size_t vsize,
                          size_t firstInstance, size_t instanceCount) {
  const VBuffer* vbo=reinterpret_cast<const VBuffer*>(ivbo);
  if(T_LIKELY(vbo!=nullptr)) {
    bindVbo(*vbo,stride);
    }
  implSetUniforms(PipelineStage::S_Graphics);
  implSetPushData(PipelineStage::S_Graphics);
  vkCmdDraw(impl, uint32_t(vsize), uint32_t(instanceCount), uint32_t(voffset), uint32_t(firstInstance));
  }

void VCommandBuffer::drawIndexed(const AbstractGraphicsApi::Buffer* ivbo, size_t stride, size_t voffset,
                                 const AbstractGraphicsApi::Buffer& iibo, Detail::IndexClass cls,
                                 size_t ioffset, size_t isize, size_t firstInstance, size_t instanceCount) {
  const VBuffer* vbo = reinterpret_cast<const VBuffer*>(ivbo);
  const VBuffer& ibo = reinterpret_cast<const VBuffer&>(iibo);
  if(T_LIKELY(vbo!=nullptr)) {
    bindVbo(*vbo,stride);
    }
  vkCmdBindIndexBuffer(impl, ibo.impl, 0, nativeFormat(cls));
  implSetUniforms(PipelineStage::S_Graphics);
  implSetPushData(PipelineStage::S_Graphics);
  vkCmdDrawIndexed    (impl, uint32_t(isize), uint32_t(instanceCount), uint32_t(ioffset), int32_t(voffset), uint32_t(firstInstance));
  }

void VCommandBuffer::drawIndirect(const AbstractGraphicsApi::Buffer& indirect, size_t offset) {
  const VBuffer& ind = reinterpret_cast<const VBuffer&>(indirect);

  // block future writers
  resState.onUavUsage(ind.nonUniqId, NonUniqResId::I_None, PipelineStage::S_Indirect);
  implSetUniforms(PipelineStage::S_Graphics);
  implSetPushData(PipelineStage::S_Graphics);
  //resState.flush(*this);
  vkCmdDrawIndirect(impl, ind.impl, VkDeviceSize(offset), 1, 0);
  }

void VCommandBuffer::dispatchMesh(size_t x, size_t y, size_t z) {
  implSetUniforms(PipelineStage::S_Graphics);
  implSetPushData(PipelineStage::S_Graphics);
  device.vkCmdDrawMeshTasks(impl, uint32_t(x), uint32_t(y), uint32_t(z));
  }

void VCommandBuffer::dispatchMeshIndirect(const AbstractGraphicsApi::Buffer& indirect, size_t offset) {
  const VBuffer& ind = reinterpret_cast<const VBuffer&>(indirect);

  // block future writers
  resState.onUavUsage(ind.nonUniqId, NonUniqResId::I_None, PipelineStage::S_Indirect);
  implSetUniforms(PipelineStage::S_Graphics);
  implSetPushData(PipelineStage::S_Graphics);
  //resState.flush(*this);
  device.vkCmdDrawMeshTasksIndirect(impl, ind.impl, VkDeviceSize(offset), 1, 0);
  }

void VCommandBuffer::bindVbo(const VBuffer& vbo, size_t stride) {
  if(curVbo!=vbo.impl) {
    VkBuffer     buffers[1] = {vbo.impl};
    VkDeviceSize offsets[1] = {0};
    vkCmdBindVertexBuffers(impl, 0, 1, buffers, offsets);
    curVbo = vbo.impl;
    }
  if(T_UNLIKELY(vboStride!=stride)) {
    vboStride      = stride;
    bindings.durty = true;
    pipelineLayout = VK_NULL_HANDLE; // maybe need to rebuild pso
    }
  }

void VCommandBuffer::setViewport(const Tempest::Rect &r) {
  VkViewport viewPort = {};
  viewPort.x        = float(r.x);
  viewPort.y        = float(r.y);
  viewPort.width    = float(r.w);
  viewPort.height   = float(r.h);
  viewPort.minDepth = 0;
  viewPort.maxDepth = 1;

  vkCmdSetViewport(impl,0,1,&viewPort);
  }

void VCommandBuffer::setScissor(const Rect& r) {
  VkRect2D scissor = {};
  scissor.offset = {r.x, r.y};
  scissor.extent = {uint32_t(r.w), uint32_t(r.h)};
  vkCmdSetScissor(impl,0,1,&scissor);
  }

void VCommandBuffer::setDebugMarker(std::string_view tag) {
  if(isDbgRegion) {
    device.vkCmdDebugMarkerEnd(impl);
    isDbgRegion = false;
    }

  if(!tag.empty()) {
    char buf[128] = {};
    std::snprintf(buf, sizeof(buf), "%.*s", int(tag.size()), tag.data());
    VkDebugMarkerMarkerInfoEXT info = {};
    info.sType       = VK_STRUCTURE_TYPE_DEBUG_MARKER_MARKER_INFO_EXT;
    info.pMarkerName = buf;
    device.vkCmdDebugMarkerBegin(impl, &info);
    isDbgRegion = true;
    }
  }

void VCommandBuffer::copy(AbstractGraphicsApi::Buffer& dstBuf, size_t offsetDest, const AbstractGraphicsApi::Buffer &srcBuf, size_t offsetSrc, size_t size) {
  auto& src = reinterpret_cast<const VBuffer&>(srcBuf);
  auto& dst = reinterpret_cast<VBuffer&>(dstBuf);

  resState.onTranferUsage(src.nonUniqId, dst.nonUniqId, dst.isHostVisible());
  resState.flush(*this);

  VkBufferCopy copyRegion = {};
  copyRegion.dstOffset = offsetDest;
  copyRegion.srcOffset = offsetSrc;
  copyRegion.size      = size;
  vkCmdCopyBuffer(impl, src.impl, dst.impl, 1, &copyRegion);
  }

void VCommandBuffer::copy(AbstractGraphicsApi::Buffer& dstBuf, size_t offsetDest, const void* src, size_t size) {
  auto& dst    = reinterpret_cast<VBuffer&>(dstBuf);
  auto  srcBuf = reinterpret_cast<const uint8_t*>(src);

  resState.onTranferUsage(NonUniqResId::I_None, dst.nonUniqId, dst.isHostVisible());
  resState.flush(*this);

  size_t maxSz = 0x10000;
  while(size>maxSz) {
    vkCmdUpdateBuffer(impl,dst.impl,offsetDest,maxSz,srcBuf);
    offsetDest += maxSz;
    srcBuf     += maxSz;
    size       -= maxSz;
    }
  vkCmdUpdateBuffer(impl,dst.impl,offsetDest,size,srcBuf);
  }

void VCommandBuffer::fill(AbstractGraphicsApi::Texture& dstTex, uint32_t val) {
  auto& dst = reinterpret_cast<VTexture&>(dstTex);

  assert(dst.nonUniqId != NonUniqResId::I_None);

  const auto resLay = dst.isStorageImage ? ResourceLayout::Default : ResourceLayout::TransferDst;
  resState.setLayout(dst, resLay, ResourceState::AllMips, true);
  resState.onTranferUsage(NonUniqResId::I_None, dst.nonUniqId, false);
  resState.flush(*this);

  VkClearColorValue v = {};
  v.uint32[0] = val;
  v.uint32[1] = val;
  v.uint32[2] = val;
  v.uint32[3] = val;

  VkImageSubresourceRange rgn = {};
  rgn.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
  rgn.baseArrayLayer = 0;
  rgn.baseMipLevel   = 0;
  rgn.levelCount     = VK_REMAINING_MIP_LEVELS;
  rgn.layerCount     = VK_REMAINING_ARRAY_LAYERS;
  vkCmdClearColorImage(impl, dst.impl, VK_IMAGE_LAYOUT_GENERAL, &v, 1, &rgn);

  if(!dst.isStorageImage)
    resState.setLayout(dst, ResourceLayout::Default, ResourceState::AllMips);
  }

void VCommandBuffer::fill(AbstractGraphicsApi::Buffer& dstBuf, size_t offsetDest, uint32_t val, size_t size) {
  auto& dst = reinterpret_cast<VBuffer&>(dstBuf);

  resState.onTranferUsage(NonUniqResId::I_None, dst.nonUniqId, dst.isHostVisible());
  resState.flush(*this);

  vkCmdFillBuffer(impl,dst.impl,offsetDest,size,val);
  }

void VCommandBuffer::copy(AbstractGraphicsApi::Texture& dstTex, size_t width, size_t height, size_t mip,
                          const AbstractGraphicsApi::Buffer& srcBuf, size_t offset) {
  auto& src = reinterpret_cast<const VBuffer&>(srcBuf);
  auto& dst = reinterpret_cast<VTexture&>(dstTex);

  assert(dst.nonUniqId != NonUniqResId::I_None);

  VkBufferImageCopy region = {};
  region.bufferOffset      = offset;
  region.bufferRowLength   = 0;
  region.bufferImageHeight = 0;
  region.imageSubresource.aspectMask = nativeIsDepthFormat(dst.format) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
  region.imageSubresource.mipLevel = uint32_t(mip);
  region.imageSubresource.baseArrayLayer = 0;
  region.imageSubresource.layerCount = 1;
  region.imageOffset = {0, 0, 0};
  region.imageExtent = {
      uint32_t(width),
      uint32_t(height),
      1
      };

  const auto resLay = dst.isStorageImage ? ResourceLayout::Default : ResourceLayout::TransferDst;
  resState.setLayout(dst, resLay, uint32_t(mip), true);
  resState.onTranferUsage(src.nonUniqId, dst.nonUniqId, false);
  resState.flush(*this);

  const VkImageLayout layout = dst.isStorageImage ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  vkCmdCopyBufferToImage(impl, src.impl, dst.impl, layout, 1, &region);

  if(!dst.isStorageImage)
    resState.setLayout(dst, ResourceLayout::Default, uint32_t(mip));
  }

void VCommandBuffer::copy(AbstractGraphicsApi::Buffer& dstBuf, size_t offset,
                          AbstractGraphicsApi::Texture& srcTex, uint32_t width, uint32_t height, uint32_t mip) {
  auto& dst = reinterpret_cast<VBuffer&>(dstBuf);
  auto& src = reinterpret_cast<VTexture&>(srcTex);

  VkBufferImageCopy region={};
  region.bufferOffset      = offset;
  region.bufferRowLength   = 0;
  region.bufferImageHeight = 0;
  region.imageSubresource.aspectMask = nativeIsDepthFormat(src.format) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
  region.imageSubresource.mipLevel = uint32_t(mip);
  region.imageSubresource.baseArrayLayer = 0;
  region.imageSubresource.layerCount = 1;
  region.imageOffset = {0, 0, 0};
  region.imageExtent = {
      uint32_t(width),
      uint32_t(height),
      1
      };

  if(!src.isStorageImage)
    resState.setLayout(src, ResourceLayout::TransferSrc, mip);
  resState.onTranferUsage(src.nonUniqId, dst.nonUniqId, dst.isHostVisible());
  resState.flush(*this);

  const VkImageLayout layout = src.isStorageImage ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
  vkCmdCopyImageToBuffer(impl, src.impl, layout, dst.impl, 1, &region);

  if(!src.isStorageImage)
    resState.setLayout(src, ResourceLayout::Default, mip);
  }

void VCommandBuffer::blit(AbstractGraphicsApi::Texture& srcTex, uint32_t srcW, uint32_t srcH, uint32_t srcMip,
                          AbstractGraphicsApi::Texture& dstTex, uint32_t dstW, uint32_t dstH, uint32_t dstMip) {
  auto& src = reinterpret_cast<VTexture&>(srcTex);
  auto& dst = reinterpret_cast<VTexture&>(dstTex);

  // blit is not guarantied by spec for depth
  assert(!nativeIsDepthFormat(src.format));
  assert(!nativeIsDepthFormat(dst.format));

  // Check if image format supports linear blitting
  VkFormatProperties formatProperties;
  vkGetPhysicalDeviceFormatProperties(device.physicalDevice, src.format, &formatProperties);

  VkFilter filter = VK_FILTER_LINEAR;
  if(0==(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)) {
    filter = VK_FILTER_NEAREST;
    }

  VkImageBlit blit = {};
  blit.srcOffsets[0] = {0, 0, 0};
  blit.srcOffsets[1] = {int32_t(srcW), int32_t(srcH), 1};
  blit.srcSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
  blit.srcSubresource.mipLevel       = srcMip;
  blit.srcSubresource.baseArrayLayer = 0;
  blit.srcSubresource.layerCount     = 1;
  blit.dstOffsets[0] = {0, 0, 0};
  blit.dstOffsets[1] = {int32_t(dstW), int32_t(dstH), 1 };
  blit.dstSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
  blit.dstSubresource.mipLevel       = dstMip;
  blit.dstSubresource.baseArrayLayer = 0;
  blit.dstSubresource.layerCount     = 1;

  vkCmdBlitImage(impl,
                 src.impl, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                 dst.impl, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                 1, &blit,
                 filter);
  }

void VCommandBuffer::buildBlas(VkAccelerationStructureKHR dest, AbstractGraphicsApi::Buffer& bbo,
                               AbstractGraphicsApi::BlasBuildCtx& rtctx, AbstractGraphicsApi::Buffer& scratch) {
  auto& ctx = reinterpret_cast<VBlasBuildCtx&>(rtctx);

  // make sure BLAS'es are ready
  resState.onUavUsage(NonUniqResId::I_None, reinterpret_cast<const VBuffer&>(bbo).nonUniqId, PipelineStage::S_RtAs);
  resState.flush(*this);

  VkAccelerationStructureBuildRangeInfoKHR* pbuildRangeInfo = ctx.ranges.data();
  auto buildGeometryInfo = ctx.buildCmd(device, dest, &reinterpret_cast<VBuffer&>(scratch));
  device.vkCmdBuildAccelerationStructures(impl, 1, &buildGeometryInfo, &pbuildRangeInfo);
  }

void VCommandBuffer::buildTlas(VkAccelerationStructureKHR dest,
                               AbstractGraphicsApi::Buffer& tbo,
                               const AbstractGraphicsApi::Buffer& instances, uint32_t numInstances,
                               AbstractGraphicsApi::Buffer& scratch) {
  VkAccelerationStructureGeometryInstancesDataKHR geometryInstancesData = {};
  geometryInstancesData.sType                 = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
  geometryInstancesData.pNext                 = NULL;
  geometryInstancesData.arrayOfPointers       = VK_FALSE;
  if(numInstances>0)
    geometryInstancesData.data.deviceAddress  = reinterpret_cast<const VBuffer&>(instances).toDeviceAddress(device);

  VkAccelerationStructureGeometryKHR geometry = {};
  geometry.sType                              = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
  geometry.pNext                              = nullptr;
  geometry.geometryType                       = VK_GEOMETRY_TYPE_INSTANCES_KHR;
  geometry.geometry.instances                 = geometryInstancesData;
  geometry.flags                              = 0;

  VkAccelerationStructureBuildGeometryInfoKHR buildGeometryInfo = {};
  buildGeometryInfo.sType                     = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
  buildGeometryInfo.pNext                     = nullptr;
  buildGeometryInfo.type                      = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR;
  buildGeometryInfo.flags                     = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR | VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR;
  buildGeometryInfo.mode                      = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
  buildGeometryInfo.srcAccelerationStructure  = VK_NULL_HANDLE;
  buildGeometryInfo.dstAccelerationStructure  = dest;
  buildGeometryInfo.geometryCount             = 1;
  buildGeometryInfo.pGeometries               = &geometry;
  buildGeometryInfo.ppGeometries              = nullptr;
  buildGeometryInfo.scratchData.deviceAddress = reinterpret_cast<const VBuffer&>(scratch).toDeviceAddress(device);

  VkAccelerationStructureBuildRangeInfoKHR buildRangeInfo = {};
  buildRangeInfo.primitiveCount               = numInstances;
  buildRangeInfo.primitiveOffset              = 0;
  buildRangeInfo.firstVertex                  = 0;
  buildRangeInfo.transformOffset              = 0;

  // make sure TLAS is ready
  resState.onUavUsage(NonUniqResId::I_None, reinterpret_cast<const VBuffer&>(tbo).nonUniqId, PipelineStage::S_RtAs);
  resState.flush(*this);

  VkAccelerationStructureBuildRangeInfoKHR* pbuildRangeInfo = &buildRangeInfo;
  device.vkCmdBuildAccelerationStructures(impl, 1, &buildGeometryInfo, &pbuildRangeInfo);
  }

void VCommandBuffer::generateMipmap(AbstractGraphicsApi::Texture& img,
                                    uint32_t texWidth, uint32_t texHeight, uint32_t mipLevels) {
  if(mipLevels==1)
    return;

  auto& image = reinterpret_cast<VTexture&>(img);
  assert(image.nonUniqId!=NonUniqResId::I_None);

  // Check if image format supports linear blitting
  VkFormatProperties formatProperties;
  vkGetPhysicalDeviceFormatProperties(device.physicalDevice, image.format, &formatProperties);

  if(!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT))
    throw std::runtime_error("texture image format does not support linear blitting!");

  int32_t w = int32_t(texWidth);
  int32_t h = int32_t(texHeight);

  for(uint32_t i=1; i<mipLevels; ++i) {
    const int mw = (w==1 ? 1 : w/2);
    const int mh = (h==1 ? 1 : h/2);

    resState.onTranferUsage(image.nonUniqId, image.nonUniqId, false);
    resState.setLayout(img, ResourceLayout::TransferDst, i+0, true);
    resState.setLayout(img, ResourceLayout::TransferSrc, i-1);
    resState.flush(*this);

    blit(img,  w, h, i-1,
         img, mw,mh, i);

    w = mw;
    h = mh;
    }
  resState.setLayout(img, ResourceLayout::Default, ResourceState::AllMips);
  }

void VCommandBuffer::discard(AbstractGraphicsApi::Texture& tex) {
  resState.forceLayout(tex,ResourceLayout::None);
  }

void VCommandBuffer::barrier(const AbstractGraphicsApi::SyncDesc& s, const AbstractGraphicsApi::BarrierDesc* desc, size_t cnt) {
  VkPipelineStageFlags srcStageMask  = 0;
  VkAccessFlags        srcAccessMask = 0;
  VkPipelineStageFlags dstStageMask  = 0;
  VkAccessFlags        dstAccessMask = 0;
  toStage(device, srcStageMask, srcAccessMask, s.prev, true);
  toStage(device, dstStageMask, dstAccessMask, s.next, false);

  VkBufferMemoryBarrier bufBarrier[MaxBarriers] = {};
  uint32_t              bufCount = 0;
  VkImageMemoryBarrier  imgBarrier[MaxBarriers] = {};
  uint32_t              imgCount = 0;
  VkMemoryBarrier       memBarrier = {};
  uint32_t              memCount = 0;

  if(s.prev!=SyncStage::None && s.next!=SyncStage::None) {
    memBarrier.sType         = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    memBarrier.srcAccessMask = srcAccessMask;
    memBarrier.dstAccessMask = dstAccessMask;
    memCount = 1;
    }

  for(size_t i=0; i<cnt; ++i) {
    auto& b = desc[i];

    auto* tx = reinterpret_cast<const VTexture*>(b.texture);
    auto& bx = imgBarrier[imgCount];
    auto* pr = imgCount>0 ? &imgBarrier[imgCount-1] : nullptr;
    ++imgCount;

    bx.sType                 = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    bx.srcQueueFamilyIndex   = VK_QUEUE_FAMILY_IGNORED;
    bx.dstQueueFamilyIndex   = VK_QUEUE_FAMILY_IGNORED;
    bx.image                 = toVkResource(b);
    bx.srcAccessMask         = srcAccessMask;
    bx.dstAccessMask         = dstAccessMask;
    bx.oldLayout             = toLayout(b.prev, tx);
    bx.newLayout             = toLayout(b.next, tx);
    finalizeImageBarrier(bx,b);

    if(bx.oldLayout==bx.newLayout) {
      --imgCount;
      continue;
      }

    // merge consecutive barriers for same resource and different mips
    if(pr!=nullptr && pr->image==bx.image &&
       pr->oldLayout==bx.oldLayout && pr->newLayout==bx.newLayout &&
       pr->subresourceRange.levelCount!=VK_REMAINING_MIP_LEVELS &&
       bx.subresourceRange.levelCount!=VK_REMAINING_MIP_LEVELS &&
       pr->subresourceRange.baseMipLevel+pr->subresourceRange.levelCount == bx.subresourceRange.baseMipLevel) {
      auto tmp = *pr;
      tmp.subresourceRange.baseMipLevel = bx.subresourceRange.baseMipLevel;
      tmp.subresourceRange.levelCount   = bx.subresourceRange.levelCount;
      if(std::memcmp(&tmp, &bx, sizeof(tmp))==0) {
        pr->subresourceRange.levelCount += bx.subresourceRange.levelCount;
        pr->srcAccessMask |= srcAccessMask;
        pr->dstAccessMask |= dstAccessMask;
        --imgCount;
        }
      }
    }

  vkCmdPipelineBarrier(impl, srcStageMask, dstStageMask, VkDependencyFlags(0),
                       memCount, &memBarrier, bufCount, bufBarrier, imgCount, imgBarrier);
  }

void VCommandBuffer::vkCmdBeginRenderingKHR(VkCommandBuffer impl, const VkRenderingInfo* info) {
  if(device.props.hasDynRendering) {
    device.vkCmdBeginRenderingKHR(impl,info);
    return;
    }

  auto fbo = device.fboMap.find(info, &passDyn);
  auto fb  = fbo.get();
  passRp   = fbo->pass;

  VkClearValue clr[MaxFramebufferAttachments];
  for(size_t i=0; i<info->colorAttachmentCount; ++i) {
    clr[i].color = info->pColorAttachments[i].clearValue.color;
    }
  if(info->pDepthAttachment!=nullptr) {
    const uint32_t i = info->colorAttachmentCount;
    clr[i].depthStencil = info->pDepthAttachment->clearValue.depthStencil;
    }

  VkRenderPassBeginInfo rinfo = {};
  rinfo.sType       = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  rinfo.renderPass  = fb->pass->pass;
  rinfo.framebuffer = fb->fbo;
  rinfo.renderArea  = info->renderArea;

  rinfo.clearValueCount   = info->colorAttachmentCount + (info->pDepthAttachment!=nullptr ? 1 : 0);
  rinfo.pClearValues      = clr;

  vkCmdBeginRenderPass(impl, &rinfo, VK_SUBPASS_CONTENTS_INLINE);
  }

void VCommandBuffer::vkCmdEndRenderingKHR(VkCommandBuffer impl) {
  if(device.props.hasDynRendering) {
    device.vkCmdEndRenderingKHR(impl);
    } else {
    vkCmdEndRenderPass(impl);
    }
  }

void VCommandBuffer::pushChunk() {
  if(impl!=nullptr) {
    vkAssert(vkEndCommandBuffer(impl));
    Chunk ch;
    ch.impl = impl;
    chunks.push(ch);
    impl = nullptr;
    }
  }

void VCommandBuffer::newChunk() {
  pushChunk();

  VkCommandBufferAllocateInfo allocInfo = {};
  allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocInfo.commandPool        = pool.impl;
  allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocInfo.commandBufferCount = 1;
  vkAssert(vkAllocateCommandBuffers(device.device.impl,&allocInfo,&impl));

  VkCommandBufferBeginInfo beginInfo = {};
  beginInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags            = 0;
  beginInfo.pInheritanceInfo = nullptr;
  vkAssert(vkBeginCommandBuffer(impl,&beginInfo));

  curVbo = VK_NULL_HANDLE;
  pushData.durty = true;
  bindings.durty = true;
  }

template<class T>
void VCommandBuffer::finalizeImageBarrier(T& bx, const AbstractGraphicsApi::BarrierDesc& b) {
  VkFormat nativeFormat = VK_FORMAT_UNDEFINED;
  if(b.texture!=nullptr) {
    auto& t = *reinterpret_cast<const VTexture*>(b.texture);
    nativeFormat = t.format;
    } else {
    auto& s = *reinterpret_cast<const VSwapchain*>(b.swapchain);
    nativeFormat = s.format();
    }

  bx.subresourceRange.baseMipLevel   = b.mip==uint32_t(-1) ? 0 : b.mip;
  bx.subresourceRange.levelCount     = b.mip==uint32_t(-1) ? VK_REMAINING_MIP_LEVELS : 1;
  bx.subresourceRange.baseArrayLayer = 0;
  bx.subresourceRange.layerCount     = VK_REMAINING_ARRAY_LAYERS;

  if(nativeFormat==VK_FORMAT_D24_UNORM_S8_UINT || nativeFormat==VK_FORMAT_D16_UNORM_S8_UINT || nativeFormat==VK_FORMAT_D32_SFLOAT_S8_UINT)
    bx.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT; else
  if(Detail::nativeIsDepthFormat(nativeFormat))
    bx.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT; else
    bx.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

  if(b.discard)
    bx.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  }

void VCommandBuffer::addDependency(VSwapchain& s, size_t imgId) {
  VSwapchain::Sync* sc = nullptr;
  for(auto& i:s.sync)
    if(i.imgId==imgId) {
      sc = &i;
      break;
      }

  for(auto i:swapchainSync)
    if(i==sc)
      return;
  assert(sc!=nullptr);
  swapchainSync.push_back(sc);
  }

#endif

