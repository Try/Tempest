#if defined(TEMPEST_BUILD_VULKAN)

#include "vcommandbuffer.h"

#include <Tempest/DescriptorSet>
#include <Tempest/Attachment>

#include "vdevice.h"
#include "vcommandpool.h"
#include "vpipeline.h"
#include "vbuffer.h"
#include "vdescriptorarray.h"
#include "vswapchain.h"
#include "vtexture.h"
#include "vframebuffermap.h"
#include "vmeshlethelper.h"

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
    }
  return VK_ATTACHMENT_STORE_OP_DONT_CARE;
  }

static void toStage(VkPipelineStageFlags2KHR& stage, VkAccessFlagBits2KHR& access, ResourceAccess rs, bool isSrc) {
  uint32_t ret = 0;
  uint32_t acc = 0;
  if((rs&ResourceAccess::TransferSrc)==ResourceAccess::TransferSrc) {
    ret |= VK_PIPELINE_STAGE_TRANSFER_BIT;
    acc |= VK_ACCESS_TRANSFER_READ_BIT;
    }
  if((rs&ResourceAccess::TransferDst)==ResourceAccess::TransferDst) {
    ret |= VK_PIPELINE_STAGE_TRANSFER_BIT;
    acc |= VK_ACCESS_TRANSFER_WRITE_BIT;
    }

  if((rs&ResourceAccess::Present)==ResourceAccess::Present) {
    ret |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    acc |= VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
    }

  if((rs&ResourceAccess::Sampler)==ResourceAccess::Sampler) {
    ret |= VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    acc |= VK_ACCESS_SHADER_READ_BIT;
    }
  if((rs&ResourceAccess::ColorAttach)==ResourceAccess::ColorAttach) {
    ret |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    acc |= (VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
    }
  if((rs&ResourceAccess::DepthAttach)==ResourceAccess::DepthAttach) {
    ret |= (VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT);
    acc |= (VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT);
    }
  if((rs&ResourceAccess::DepthReadOnly)==ResourceAccess::DepthReadOnly) {
    ret |= VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    acc |= VK_ACCESS_SHADER_READ_BIT;
    }

  if((rs&ResourceAccess::Index)==ResourceAccess::Index) {
    ret |= VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
    acc |= VK_ACCESS_INDEX_READ_BIT;
    }
  if((rs&ResourceAccess::Vertex)==ResourceAccess::Vertex) {
    ret |= VK_PIPELINE_STAGE_VERTEX_INPUT_BIT;
    acc |= VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
    }
  if((rs&ResourceAccess::Vertex)==ResourceAccess::Uniform) {
    ret |= VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    acc |= VK_ACCESS_UNIFORM_READ_BIT;
    }

  // memory barriers
  if((rs&ResourceAccess::UavReadGr)==ResourceAccess::UavReadGr) {
    ret |= VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;
    acc |= VK_ACCESS_SHADER_READ_BIT;
    }
  if((rs&ResourceAccess::UavWriteGr)==ResourceAccess::UavWriteGr) {
    ret |= VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT;
    acc |= VK_ACCESS_SHADER_WRITE_BIT;
    }

  if((rs&ResourceAccess::UavReadComp)==ResourceAccess::UavReadComp) {
    ret |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    acc |= VK_ACCESS_SHADER_READ_BIT;
    }
  if((rs&ResourceAccess::UavWriteComp)==ResourceAccess::UavWriteComp) {
    ret |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    acc |= VK_ACCESS_SHADER_WRITE_BIT;
    }

  if(isSrc && ret==0) {
    // wait for nothing: asset uploading case
    ret = VK_PIPELINE_STAGE_NONE_KHR;
    acc = VK_ACCESS_NONE_KHR;
    }

  stage  = VkPipelineStageFlags2KHR(ret);
  access = VkAccessFlagBits2KHR(acc);
  }

static VkImageLayout toLayout(ResourceAccess rs) {
  if(rs==ResourceAccess::None)
    return VK_IMAGE_LAYOUT_UNDEFINED;

  if((rs&ResourceAccess::TransferSrc)==ResourceAccess::TransferSrc)
    return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
  if((rs&ResourceAccess::TransferDst)==ResourceAccess::TransferDst)
    return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

  if((rs&ResourceAccess::Present)==ResourceAccess::Present)
    return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

  if((rs&ResourceAccess::Sampler)==ResourceAccess::Sampler)
    return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  if((rs&ResourceAccess::ColorAttach)==ResourceAccess::ColorAttach)
    return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  if((rs&ResourceAccess::DepthReadOnly)==ResourceAccess::DepthReadOnly)
    return VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
  if((rs&ResourceAccess::DepthAttach)==ResourceAccess::DepthAttach)
    return VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;

  return VK_IMAGE_LAYOUT_GENERAL;
  }

static VkImage toVkResource(const AbstractGraphicsApi::BarrierDesc& b) {
  if(b.texture!=nullptr) {
    VTexture& t = *reinterpret_cast<VTexture*>(b.texture);
    return t.impl;
    }

  if(b.buffer!=nullptr) {
    VBuffer& buf = *reinterpret_cast<VBuffer*>(b.buffer);
    return buf.impl;
    }

  VSwapchain& s = *reinterpret_cast<VSwapchain*>(b.swapchain);
  return s.images[b.swId];
  }


VCommandBuffer::VCommandBuffer(VDevice& device, VkCommandPoolCreateFlags flags)
  :device(device), pool(device,flags) {
  }

VCommandBuffer::~VCommandBuffer() {
  if(chunks.size()==0)
    return;

  SmallArray<VkCommandBuffer,64> flat(chunks.size());
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

  SmallArray<VkCommandBuffer,64> flat(chunks.size());
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
  }

void VCommandBuffer::begin() {
  state  = Idle;
  curVbo = VK_NULL_HANDLE;
  if(chunks.size()>0)
    reset();
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

void VCommandBuffer::end() {
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

  if(state!=Idle) {
    newChunk();
    }

  resState.joinCompute(PipelineStage::S_Graphics);
  resState.setRenderpass(*this,desc,descSize,frm,att,sw,imgId);

  if(device.props.hasDynRendering) {
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

    VkImageView imageView   = VK_NULL_HANDLE;
    VkFormat    imageFormat = VK_FORMAT_UNDEFINED;
    for(size_t i = 0; i < descSize; ++i) {
      if(sw[i] != nullptr) {
        auto& t = *reinterpret_cast<VSwapchain*>(sw[i]);
        imageView   = t.views[imgId[i]];
        imageFormat = t.format();
        }
      else if (desc[i].attachment != nullptr) {
        auto& t = *reinterpret_cast<VTexture*>(att[i]);
        imageView   = t.imgView;
        imageFormat = t.format;
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
        info.pDepthAttachment = &depthAtt;
        att.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
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

    device.vkCmdBeginRenderingKHR(impl,&info);
    } else {
    auto fbo = device.fboMap.find(desc,descSize, att,sw,imgId,width,height);
    auto fb  = fbo.get();
    pass = fbo->pass;

    VkClearValue clr[MaxFramebufferAttachments];
    for(size_t i=0; i<descSize; ++i) {
      if(isDepthFormat(frm[i])) {
        clr[i].depthStencil.depth = desc[i].clear.x;
        } else {
        clr[i].color.float32[0]   = desc[i].clear.x;
        clr[i].color.float32[1]   = desc[i].clear.y;
        clr[i].color.float32[2]   = desc[i].clear.z;
        clr[i].color.float32[3]   = desc[i].clear.w;
        }
      }

    VkRenderPassBeginInfo info = {};
    info.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    info.renderPass        = fb->pass->pass;
    info.framebuffer       = fb->fbo;
    info.renderArea.offset = {0, 0};
    info.renderArea.extent = {width,height};

    info.clearValueCount   = uint32_t(descSize);
    info.pClearValues      = clr;

    vkCmdBeginRenderPass(impl, &info, VK_SUBPASS_CONTENTS_INLINE);
    }
  state = RenderPass;

  // setup dynamic state
  // https://www.khronos.org/registry/vulkan/specs/1.1-extensions/html/vkspec.html#pipelines-dynamic-state
  setViewport(Rect(0,0,int32_t(width),int32_t(height)));

  VkRect2D scissor = {};
  scissor.offset = {0, 0};
  scissor.extent = {width, height};
  vkCmdSetScissor(impl,0,1,&scissor);
  }

void VCommandBuffer::endRendering() {
  if(device.props.hasDynRendering) {
    device.vkCmdEndRenderingKHR(impl);
    } else {
    vkCmdEndRenderPass(impl);
    }
  state = PostRenderPass;
  }

void VCommandBuffer::setPipeline(AbstractGraphicsApi::Pipeline& p) {
  VPipeline& px   = reinterpret_cast<VPipeline&>(p);
  curDrawPipeline = &px;
  vboStride       = px.defaultStride;

  VkPipeline v  = device.props.hasDynRendering ? px.instance(passDyn,vboStride) : px.instance(pass,vboStride);
  vkCmdBindPipeline(impl,VK_PIPELINE_BIND_POINT_GRAPHICS,v);
  }

void VCommandBuffer::setBytes(AbstractGraphicsApi::Pipeline& p, const void* data, size_t size) {
  VPipeline&        px=reinterpret_cast<VPipeline&>(p);
  assert(size<=px.pushSize);
  vkCmdPushConstants(impl, px.pipelineLayout, px.pushStageFlags, 0, uint32_t(size), data);
  }

void VCommandBuffer::setUniforms(AbstractGraphicsApi::Pipeline &p, AbstractGraphicsApi::Desc &u) {
  VPipeline&        px=reinterpret_cast<VPipeline&>(p);
  VDescriptorArray& ux=reinterpret_cast<VDescriptorArray&>(u);
  curUniforms = &ux;
  ux.ssboBarriers(resState,PipelineStage::S_Graphics);
  vkCmdBindDescriptorSets(impl,VK_PIPELINE_BIND_POINT_GRAPHICS,
                          px.pipelineLayout,0,
                          1,&ux.impl,
                          0,nullptr);
  }

void VCommandBuffer::setComputePipeline(AbstractGraphicsApi::CompPipeline& p) {
  state = Compute;
  VCompPipeline& px = reinterpret_cast<VCompPipeline&>(p);
  vkCmdBindPipeline(impl,VK_PIPELINE_BIND_POINT_COMPUTE,px.impl);
  }

void VCommandBuffer::dispatch(size_t x, size_t y, size_t z) {
  curUniforms->ssboBarriers(resState,PipelineStage::S_Compute);
  resState.flush(*this);
  vkCmdDispatch(impl,uint32_t(x),uint32_t(y),uint32_t(z));
  }

void VCommandBuffer::setBytes(AbstractGraphicsApi::CompPipeline& p, const void* data, size_t size) {
  VCompPipeline& px=reinterpret_cast<VCompPipeline&>(p);
  assert(size<=px.pushSize);
  vkCmdPushConstants(impl, px.pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, uint32_t(size), data);
  }

void VCommandBuffer::setUniforms(AbstractGraphicsApi::CompPipeline& p, AbstractGraphicsApi::Desc& u) {
  VCompPipeline&    px=reinterpret_cast<VCompPipeline&>(p);
  VDescriptorArray& ux=reinterpret_cast<VDescriptorArray&>(u);
  curUniforms = &ux;
  vkCmdBindDescriptorSets(impl,VK_PIPELINE_BIND_POINT_COMPUTE,
                          px.pipelineLayout,0,
                          1,&ux.impl,
                          0,nullptr);
  }

void VCommandBuffer::draw(size_t vsize, size_t firstInstance, size_t instanceCount) {
  vkCmdDraw(impl, uint32_t(vsize), uint32_t(instanceCount), 0, uint32_t(firstInstance));
  }

void VCommandBuffer::draw(const AbstractGraphicsApi::Buffer& ivbo, size_t stride, size_t voffset, size_t vsize,
                          size_t firstInstance, size_t instanceCount) {
  const VBuffer& vbo=reinterpret_cast<const VBuffer&>(ivbo);
  bindVbo(vbo,stride);
  vkCmdDraw(impl, uint32_t(vsize), uint32_t(instanceCount), uint32_t(voffset), uint32_t(firstInstance));
  }

void VCommandBuffer::drawIndexed(const AbstractGraphicsApi::Buffer& ivbo, size_t stride, size_t voffset,
                                 const AbstractGraphicsApi::Buffer& iibo, Detail::IndexClass cls,
                                 size_t ioffset, size_t isize, size_t firstInstance, size_t instanceCount) {
  const VBuffer& vbo = reinterpret_cast<const VBuffer&>(ivbo);
  const VBuffer& ibo = reinterpret_cast<const VBuffer&>(iibo);
  bindVbo(vbo,stride);
  vkCmdBindIndexBuffer(impl, ibo.impl, 0, nativeFormat(cls));
  vkCmdDrawIndexed    (impl, uint32_t(isize), uint32_t(instanceCount), uint32_t(ioffset), int32_t(voffset), uint32_t(firstInstance));
  }

void VCommandBuffer::dispatchMesh(size_t firstInstance, size_t instanceCount) {
  device.vkCmdDrawMeshTasks(impl, uint32_t(instanceCount), uint32_t(firstInstance));
  }

void VCommandBuffer::bindVbo(const VBuffer& vbo, size_t stride) {
  if(curVbo!=vbo.impl) {
    if(T_UNLIKELY(vboStride!=stride)) {
      auto& px = *curDrawPipeline;
      vboStride = stride;
      VkPipeline v  = device.props.hasDynRendering ? px.instance(passDyn,vboStride) : px.instance(pass,vboStride);
      vkCmdBindPipeline(impl,VK_PIPELINE_BIND_POINT_GRAPHICS,v);
      }
    VkBuffer     buffers[1] = {vbo.impl};
    VkDeviceSize offsets[1] = {0};
    vkCmdBindVertexBuffers(impl, 0, 1, buffers, offsets);
    curVbo = vbo.impl;
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

void VCommandBuffer::copy(AbstractGraphicsApi::Buffer& dstBuf, size_t offsetDest, const AbstractGraphicsApi::Buffer &srcBuf, size_t offsetSrc, size_t size) {
  auto& src = reinterpret_cast<const VBuffer&>(srcBuf);
  auto& dst = reinterpret_cast<VBuffer&>(dstBuf);

  VkBufferCopy copyRegion = {};
  copyRegion.dstOffset = offsetDest;
  copyRegion.srcOffset = offsetSrc;
  copyRegion.size      = size;
  vkCmdCopyBuffer(impl, src.impl, dst.impl, 1, &copyRegion);
  }

void VCommandBuffer::copy(AbstractGraphicsApi::Buffer& dstBuf, size_t offsetDest, const void* src, size_t size) {
  auto&  dst    = reinterpret_cast<VBuffer&>(dstBuf);

  auto   srcBuf = reinterpret_cast<const uint8_t*>(src);
  size_t maxSz  = 0x10000;
  while(size>maxSz) {
    vkCmdUpdateBuffer(impl,dst.impl,offsetDest,maxSz,srcBuf);
    offsetDest += maxSz;
    srcBuf     += maxSz;
    size       -= maxSz;
    }
  vkCmdUpdateBuffer(impl,dst.impl,offsetDest,size,srcBuf);

  VkBufferMemoryBarrier buf = {};
  buf.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
  buf.srcAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT;
  buf.dstAccessMask       = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
  buf.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  buf.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  buf.buffer              = dst.impl;
  buf.offset              = 0;
  buf.size                = VK_WHOLE_SIZE;

  vkCmdPipelineBarrier(impl, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                       0,
                       0, nullptr,
                       1, &buf,
                       0, nullptr);
  }

void VCommandBuffer::copy(AbstractGraphicsApi::Texture& dstTex, size_t width, size_t height, size_t mip, const AbstractGraphicsApi::Buffer& srcBuf, size_t offset) {
  auto& src = reinterpret_cast<const VBuffer&>(srcBuf);
  auto& dst = reinterpret_cast<VTexture&>(dstTex);

  VkBufferImageCopy region = {};
  region.bufferOffset      = offset;
  region.bufferRowLength   = 0;
  region.bufferImageHeight = 0;
  region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  region.imageSubresource.mipLevel = uint32_t(mip);
  region.imageSubresource.baseArrayLayer = 0;
  region.imageSubresource.layerCount = 1;
  region.imageOffset = {0, 0, 0};
  region.imageExtent = {
      uint32_t(width),
      uint32_t(height),
      1
  };

  vkCmdCopyBufferToImage(impl, src.impl, dst.impl, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
  }

void VCommandBuffer::copyNative(AbstractGraphicsApi::Buffer&        dst, size_t offset,
                                const AbstractGraphicsApi::Texture& src, size_t width, size_t height, size_t mip) {
  auto& nSrc = reinterpret_cast<const VTexture&>(src);
  auto& nDst = reinterpret_cast<VBuffer&>(dst);

  VkBufferImageCopy region={};
  region.bufferOffset      = offset;
  region.bufferRowLength   = 0;
  region.bufferImageHeight = 0;
  region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  region.imageSubresource.mipLevel = uint32_t(mip);
  region.imageSubresource.baseArrayLayer = 0;
  region.imageSubresource.layerCount = 1;
  region.imageOffset = {0, 0, 0};
  region.imageExtent = {
      uint32_t(width),
      uint32_t(height),
      1
  };

  VkImageLayout layout =  nSrc.isStorageImage ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
  vkCmdCopyImageToBuffer(impl, nSrc.impl, layout, nDst.impl, 1, &region);
  }

void VCommandBuffer::blit(AbstractGraphicsApi::Texture& srcTex, uint32_t srcW, uint32_t srcH, uint32_t srcMip,
                          AbstractGraphicsApi::Texture& dstTex, uint32_t dstW, uint32_t dstH, uint32_t dstMip) {
  auto& src = reinterpret_cast<VTexture&>(srcTex);
  auto& dst = reinterpret_cast<VTexture&>(dstTex);

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

void VCommandBuffer::buildBlas(VkAccelerationStructureKHR dest,
                               const AbstractGraphicsApi::Buffer& ivbo, size_t vboSz, size_t stride,
                               const AbstractGraphicsApi::Buffer& iibo, size_t iboSz, size_t ioffset, IndexClass icls,
                               AbstractGraphicsApi::Buffer& scratch) {
  auto& vbo = reinterpret_cast<const VBuffer&>(ivbo);
  auto& ibo = reinterpret_cast<const VBuffer&>(iibo);

  VkAccelerationStructureGeometryKHR geometry = {};
  geometry.sType                              = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
  geometry.pNext                              = nullptr;
  geometry.geometryType                       = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
  geometry.geometry.triangles.sType           = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
  geometry.geometry.triangles.vertexFormat    = VK_FORMAT_R32G32B32_SFLOAT;
  geometry.geometry.triangles.vertexStride    = stride;
  geometry.geometry.triangles.maxVertex       = uint32_t(vboSz);
  geometry.geometry.triangles.indexType       = nativeFormat(icls);
  geometry.geometry.triangles.transformData   = VkDeviceOrHostAddressConstKHR{};
  geometry.flags                              = VK_GEOMETRY_OPAQUE_BIT_KHR;

  geometry.geometry.triangles.vertexData.deviceAddress = vbo.toDeviceAddress(device);
  geometry.geometry.triangles.indexData .deviceAddress = ibo.toDeviceAddress(device);

  VkAccelerationStructureBuildGeometryInfoKHR buildGeometryInfo = {};
  buildGeometryInfo.sType                     = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
  buildGeometryInfo.pNext                     = nullptr;
  buildGeometryInfo.type                      = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR;
  buildGeometryInfo.flags                     = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR;
  buildGeometryInfo.mode                      = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
  buildGeometryInfo.srcAccelerationStructure  = VK_NULL_HANDLE;
  buildGeometryInfo.dstAccelerationStructure  = dest;
  buildGeometryInfo.geometryCount             = 1;
  buildGeometryInfo.pGeometries               = &geometry;
  buildGeometryInfo.ppGeometries              = nullptr;
  buildGeometryInfo.scratchData.deviceAddress = reinterpret_cast<const VBuffer&>(scratch).toDeviceAddress(device);

  VkAccelerationStructureBuildRangeInfoKHR buildRangeInfo = {};
  buildRangeInfo.primitiveCount               = uint32_t(iboSz/3);
  buildRangeInfo.primitiveOffset              = uint32_t(ioffset*sizeofIndex(icls));
  buildRangeInfo.firstVertex                  = 0;
  buildRangeInfo.transformOffset              = 0;

  VkAccelerationStructureBuildRangeInfoKHR* pbuildRangeInfo = &buildRangeInfo;
  device.vkCmdBuildAccelerationStructures(impl, 1, &buildGeometryInfo, &pbuildRangeInfo);

  // make sure BLAS'es are ready
  VkMemoryBarrier barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
  barrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
  barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
  vkCmdPipelineBarrier(impl,
                       VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                       VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                       0, 1, &barrier, 0, nullptr, 0, nullptr);
  }

void VCommandBuffer::buildTlas(VkAccelerationStructureKHR dest,
                               AbstractGraphicsApi::Buffer& tbo,
                               const AbstractGraphicsApi::Buffer& instances, uint32_t numInstances,
                               AbstractGraphicsApi::Buffer& scratch) {
  VkAccelerationStructureGeometryInstancesDataKHR geometryInstancesData = {};
  geometryInstancesData.sType                 = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
  geometryInstancesData.pNext                 = NULL;
  geometryInstancesData.arrayOfPointers       = VK_FALSE;
  geometryInstancesData.data.deviceAddress    = reinterpret_cast<const VBuffer&>(instances).toDeviceAddress(device);

  VkAccelerationStructureGeometryKHR geometry = {};
  geometry.sType                              = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
  geometry.pNext                              = nullptr;
  geometry.geometryType                       = VK_GEOMETRY_TYPE_INSTANCES_KHR;
  geometry.geometry.instances                 = geometryInstancesData;
  geometry.flags                              = VK_GEOMETRY_OPAQUE_BIT_KHR;

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

  VkAccelerationStructureBuildRangeInfoKHR* pbuildRangeInfo = &buildRangeInfo;
  device.vkCmdBuildAccelerationStructures(impl, 1, &buildGeometryInfo, &pbuildRangeInfo);

  // make sure TLAS is ready
  VkMemoryBarrier barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER};
  barrier.srcAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR;
  barrier.dstAccessMask = VK_ACCESS_ACCELERATION_STRUCTURE_READ_BIT_KHR;
  vkCmdPipelineBarrier(impl,
                       VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                       VK_PIPELINE_STAGE_ACCELERATION_STRUCTURE_BUILD_BIT_KHR,
                       0, 1, &barrier, 0, nullptr, 0, nullptr);
  }

void VCommandBuffer::copy(AbstractGraphicsApi::Buffer& dst, size_t offset,
                          AbstractGraphicsApi::Texture& src, uint32_t width, uint32_t height, uint32_t mip) {
  auto& nDst = reinterpret_cast<VBuffer&>(dst);
  auto& nSrc = reinterpret_cast<const VTexture&>(src);
  if(!nSrc.isStorageImage)
    resState.setLayout(src,ResourceAccess::TransferSrc);
  resState.onTranferUsage(nDst.nonUniqId,nSrc.nonUniqId);
  resState.flush(*this);
  copyNative(dst,offset, src,width,height,mip);
  if(!nSrc.isStorageImage)
    resState.setLayout(src,ResourceAccess::Sampler);
  }

void VCommandBuffer::generateMipmap(AbstractGraphicsApi::Texture& img,
                                    uint32_t texWidth, uint32_t texHeight, uint32_t mipLevels) {
  if(mipLevels==1)
    return;

  auto& image = reinterpret_cast<VTexture&>(img);

  // Check if image format supports linear blitting
  VkFormatProperties formatProperties;
  vkGetPhysicalDeviceFormatProperties(device.physicalDevice, image.format, &formatProperties);

  if(!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT))
    throw std::runtime_error("texture image format does not support linear blitting!");

  int32_t w = int32_t(texWidth);
  int32_t h = int32_t(texHeight);

  resState.setLayout(img,ResourceAccess::TransferDst);
  resState.flush(*this);

  for(uint32_t i=1; i<mipLevels; ++i) {
    const int mw = (w==1 ? 1 : w/2);
    const int mh = (h==1 ? 1 : h/2);

    barrier(img,ResourceAccess::TransferDst, ResourceAccess::TransferSrc,i-1);
    blit(img,  w, h, i-1,
         img, mw,mh, i);

    w = mw;
    h = mh;
    }
  barrier(img,ResourceAccess::TransferDst, ResourceAccess::TransferSrc, mipLevels-1);
  barrier(img,ResourceAccess::TransferSrc, ResourceAccess::Sampler,     uint32_t(-1));
  resState.setLayout(img,ResourceAccess::Sampler);
  resState.forceLayout(img);
  }

void VCommandBuffer::barrier(const AbstractGraphicsApi::BarrierDesc* desc, size_t cnt) {
  VkBufferMemoryBarrier2KHR bufBarrier[MaxBarriers] = {};
  uint32_t                  bufCount = 0;
  VkImageMemoryBarrier2KHR  imgBarrier[MaxBarriers] = {};
  uint32_t                  imgCount = 0;
  VkMemoryBarrier2KHR       memBarrier = {};

  VkDependencyInfoKHR info = {};
  info.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO_KHR;

  for(size_t i=0; i<cnt; ++i) {
    auto& b  = desc[i];

    if(b.buffer==nullptr && b.texture==nullptr && b.swapchain==nullptr) {
      VkPipelineStageFlags2KHR srcStageMask  = 0;
      VkAccessFlags2KHR        srcAccessMask = 0;
      VkPipelineStageFlags2KHR dstStageMask  = 0;
      VkAccessFlags2KHR        dstAccessMask = 0;
      toStage(srcStageMask, srcAccessMask, b.prev,true);
      toStage(dstStageMask, dstAccessMask, b.next,false);

      memBarrier.sType          = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2_KHR;
      memBarrier.srcStageMask  |= srcStageMask;
      memBarrier.srcAccessMask |= srcAccessMask;
      memBarrier.dstStageMask  |= dstStageMask;
      memBarrier.dstAccessMask |= dstAccessMask;
      }
    else if(b.buffer!=nullptr) {
      auto& bx = bufBarrier[bufCount];
      ++bufCount;

      bx.sType                 = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2_KHR;
      bx.srcQueueFamilyIndex   = VK_QUEUE_FAMILY_IGNORED;
      bx.dstQueueFamilyIndex   = VK_QUEUE_FAMILY_IGNORED;
      bx.buffer                = reinterpret_cast<VBuffer&>(*b.buffer).impl;
      bx.offset                = 0;
      bx.size                  = VK_WHOLE_SIZE;

      toStage(bx.srcStageMask, bx.srcAccessMask, b.prev,true);
      toStage(bx.dstStageMask, bx.dstAccessMask, b.next,false);
      } else {
      auto& bx = imgBarrier[imgCount];
      ++imgCount;

      bx.sType                 = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2_KHR;
      bx.srcQueueFamilyIndex   = VK_QUEUE_FAMILY_IGNORED;
      bx.dstQueueFamilyIndex   = VK_QUEUE_FAMILY_IGNORED;
      bx.image                 = toVkResource(b);

      toStage(bx.srcStageMask, bx.srcAccessMask, b.prev,true);
      toStage(bx.dstStageMask, bx.dstAccessMask, b.next,false);

      bx.oldLayout             = toLayout(b.prev);
      bx.newLayout             = toLayout(b.next);
      finalizeImageBarrier(bx,b);
      }
    }

  info.pBufferMemoryBarriers    = bufBarrier;
  info.bufferMemoryBarrierCount = bufCount;
  info.pImageMemoryBarriers     = imgBarrier;
  info.imageMemoryBarrierCount  = imgCount;
  if(memBarrier.sType==VK_STRUCTURE_TYPE_MEMORY_BARRIER_2_KHR) {
    info.pMemoryBarriers = &memBarrier;
    info.memoryBarrierCount++;
    }

  vkCmdPipelineBarrier2(impl,&info);
  }

void VCommandBuffer::vkCmdPipelineBarrier2(VkCommandBuffer impl, const VkDependencyInfoKHR* info) {
  if(device.vkCmdPipelineBarrier2!=nullptr) {
    device.vkCmdPipelineBarrier2(impl,info);
    return;
    }

  VkPipelineStageFlags  srcStage = 0;
  VkPipelineStageFlags  dstStage = 0;

  uint32_t              memCount = 0;
  VkMemoryBarrier       memBarrier[MaxBarriers] = {};
  uint32_t              bufCount = 0;
  VkBufferMemoryBarrier bufBarrier[MaxBarriers] = {};
  uint32_t              imgCount = 0;
  VkImageMemoryBarrier  imgBarrier[MaxBarriers] = {};

  for(size_t i=0; i<info->memoryBarrierCount; ++i) {
    auto& b = info->pMemoryBarriers[i];

    if(memCount>MaxBarriers || b.srcStageMask!=srcStage || b.dstStageMask!=dstStage) {
      if(memCount>0) {
        vkCmdPipelineBarrier(impl,srcStage,dstStage,info->dependencyFlags,
                             memCount,memBarrier,
                             0,nullptr,
                             0,nullptr);
        }
      srcStage = VkPipelineStageFlags(b.srcStageMask);
      dstStage = VkPipelineStageFlags(b.dstStageMask);
      memCount = 0;
      }

    auto& bx = memBarrier[memCount];
    bx.sType         = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    bx.srcAccessMask = VkAccessFlags(b.srcAccessMask);
    bx.dstAccessMask = VkAccessFlags(b.dstAccessMask);
    ++memCount;
    }

  for(size_t i=0; i<info->bufferMemoryBarrierCount; ++i) {
    auto& b = info->pBufferMemoryBarriers[i];

    if(bufCount>MaxBarriers || b.srcStageMask!=srcStage || b.dstStageMask!=dstStage) {
      if(bufCount>0 || memCount>0) {
        vkCmdPipelineBarrier(impl,srcStage,dstStage,info->dependencyFlags,
                             memCount,memBarrier,
                             bufCount,bufBarrier,
                             0,nullptr);
        }
      srcStage = VkPipelineStageFlags(b.srcStageMask);
      dstStage = VkPipelineStageFlags(b.dstStageMask);
      memCount = 0;
      bufCount = 0;
      }

    auto& bx = bufBarrier[bufCount];
    bx.sType                 = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
    bx.srcAccessMask         = VkAccessFlags(b.srcAccessMask);
    bx.dstAccessMask         = VkAccessFlags(b.dstAccessMask);
    bx.srcQueueFamilyIndex   = b.srcQueueFamilyIndex;
    bx.dstQueueFamilyIndex   = b.dstQueueFamilyIndex;
    bx.buffer                = b.buffer;
    bx.offset                = b.offset;
    bx.size                  = b.size;
    ++bufCount;
    }

  for(size_t i=0; i<info->imageMemoryBarrierCount; ++i) {
    auto& b = info->pImageMemoryBarriers[i];

    if(imgCount>MaxBarriers || b.srcStageMask!=srcStage || b.dstStageMask!=dstStage) {
      if(imgCount>0 || bufCount>0 || memCount>0) {
        vkCmdPipelineBarrier(impl,srcStage,dstStage,info->dependencyFlags,
                             memCount,memBarrier,
                             bufCount,bufBarrier,
                             imgCount,imgBarrier);
        }
      srcStage = VkPipelineStageFlags(b.srcStageMask);
      dstStage = VkPipelineStageFlags(b.dstStageMask);
      memCount = 0;
      bufCount = 0;
      imgCount = 0;
      }

    auto& bx = imgBarrier[imgCount];
    bx.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    bx.srcAccessMask       = VkAccessFlags(b.srcAccessMask);
    bx.dstAccessMask       = VkAccessFlags(b.dstAccessMask);
    bx.oldLayout           = b.oldLayout;
    bx.newLayout           = b.newLayout;
    bx.srcQueueFamilyIndex = b.srcQueueFamilyIndex;
    bx.dstQueueFamilyIndex = b.dstQueueFamilyIndex;
    bx.image               = b.image;
    bx.subresourceRange    = b.subresourceRange;
    ++imgCount;
    }

  if(memCount==0 && bufCount==0 && imgCount==0)
    return;

  if(srcStage==0)
    srcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;

  vkCmdPipelineBarrier(impl,srcStage,dstStage,info->dependencyFlags,
                       memCount,memBarrier,
                       bufCount,bufBarrier,
                       imgCount,imgBarrier);
  }

void VCommandBuffer::pushChunk() {
  if(cbHelper!=nullptr) {
    auto& ms = *device.meshHelper;
    ms.sortPass(cbHelper,meshIndirectId);

    vkAssert(vkEndCommandBuffer(cbHelper));
    Chunk ch;
    ch.impl = cbHelper;
    chunks.push(ch);
    cbHelper = nullptr;
    }

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

  curVbo      = VK_NULL_HANDLE;
  curUniforms = nullptr;
  }

template<class T>
void VCommandBuffer::finalizeImageBarrier(T& bx, const AbstractGraphicsApi::BarrierDesc& b) {
  if(b.prev==ResourceAccess::Present)
    bx.oldLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
  if(b.next==ResourceAccess::Present)
    bx.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

  VkFormat nativeFormat = VK_FORMAT_UNDEFINED;
  if(b.texture!=nullptr) {
    VTexture& t   = *reinterpret_cast<VTexture*>  (b.texture);
    nativeFormat  = t.format;
    } else {
    VSwapchain& s = *reinterpret_cast<VSwapchain*>(b.swapchain);
    nativeFormat  = s.format();
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


void VMeshCommandBuffer::setPipeline(AbstractGraphicsApi::Pipeline& p) {
  VPipeline& px = reinterpret_cast<VPipeline&>(p);
  VCommandBuffer::setPipeline(px);
  if(px.meshPipeline()==VK_NULL_HANDLE)
    return;

  auto& ms = *device.meshHelper;

  if(cbHelper==VK_NULL_HANDLE) {
    meshIndirectId = 0;

    VkCommandBufferAllocateInfo allocInfo = {};
    allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool        = pool.impl;
    allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;
    vkAssert(vkAllocateCommandBuffers(device.device.impl,&allocInfo,&cbHelper));

    VkCommandBufferBeginInfo beginInfo = {};
    beginInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags            = 0;
    beginInfo.pInheritanceInfo = nullptr;
    vkAssert(vkBeginCommandBuffer(cbHelper,&beginInfo));

    ms.initRP(cbHelper);
    }

  vkCmdBindPipeline(cbHelper,VK_PIPELINE_BIND_POINT_COMPUTE,px.meshPipeline());
  ms.bindCS(cbHelper,px.meshPipelineLayout());
  ms.bindVS(impl,    px.pipelineLayout);
  }

void VMeshCommandBuffer::setBytes(AbstractGraphicsApi::Pipeline& p, const void* data, size_t size) {
  VCommandBuffer::setBytes(p,data,size);

  VPipeline& px = reinterpret_cast<VPipeline&>(p);
  if(px.meshPipeline()==VK_NULL_HANDLE)
    return;
  vkCmdPushConstants(cbHelper, px.meshPipelineLayout(), VK_SHADER_STAGE_COMPUTE_BIT, 0, uint32_t(size), data);
  }

void VMeshCommandBuffer::setUniforms(AbstractGraphicsApi::Pipeline& p, AbstractGraphicsApi::Desc& u) {
  VCommandBuffer::setUniforms(p,u);

  VPipeline& px = reinterpret_cast<VPipeline&>(p);
  if(px.meshPipeline()==VK_NULL_HANDLE)
    return;

  VDescriptorArray& ux=reinterpret_cast<VDescriptorArray&>(u);
  vkCmdBindDescriptorSets(cbHelper,VK_PIPELINE_BIND_POINT_COMPUTE,
                          px.meshPipelineLayout(),0,
                          1,&ux.impl,
                          0,nullptr);
  }

void VMeshCommandBuffer::dispatchMesh(size_t firstInstance, size_t instanceCount) {
  VPipeline& px = reinterpret_cast<VPipeline&>(*curDrawPipeline);
  if(px.meshPipeline()==VK_NULL_HANDLE)
    return;

  auto& ms = *device.meshHelper;
  device.vkCmdDispatchBase(cbHelper,
                           firstInstance, meshIndirectId, 0,
                           instanceCount, 1, 1);
  ms.drawIndirect(impl, meshIndirectId);
  ++meshIndirectId;
  }

#endif
