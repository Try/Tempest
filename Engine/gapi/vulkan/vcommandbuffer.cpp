#if defined(TEMPEST_BUILD_VULKAN)

#include "vcommandbuffer.h"

#include "vdevice.h"
#include "vcommandpool.h"
#include "vframebuffer.h"
#include "vframebufferlayout.h"
#include "vrenderpass.h"
#include "vpipeline.h"
#include "vbuffer.h"
#include "vdescriptorarray.h"
#include "vswapchain.h"
#include "vtexture.h"

using namespace Tempest;
using namespace Tempest::Detail;

VCommandBuffer::VCommandBuffer(VDevice& device, VkCommandPoolCreateFlags flags)
  :device(device), pool(device,flags) {
  VkCommandBufferAllocateInfo allocInfo = {};
  allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocInfo.commandPool        = pool.impl;
  allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocInfo.commandBufferCount = 1;

  vkAssert(vkAllocateCommandBuffers(device.device.impl,&allocInfo,&impl));
  }

VCommandBuffer::~VCommandBuffer() {
  vkFreeCommandBuffers(device.device.impl,pool.impl,1,&impl);
  }

void VCommandBuffer::reset() {
  vkResetCommandBuffer(impl,VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);
  swapchainSync.reserve(swapchainSync.size());
  swapchainSync.clear();
  }

void VCommandBuffer::begin() {
  begin(0);
  }

void VCommandBuffer::begin(VkCommandBufferUsageFlags flg) {
  state = NoPass;
  swapchainSync.reserve(swapchainSync.size());
  swapchainSync.clear();
  curVbo = VK_NULL_HANDLE;

  VkCommandBufferBeginInfo beginInfo = {};
  beginInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags            = flg;
  beginInfo.pInheritanceInfo = nullptr;

  vkAssert(vkBeginCommandBuffer(impl,&beginInfo));
  }

void VCommandBuffer::end() {
  if(state==RenderPass)
    endRenderPass();
  swapchainSync.reserve(swapchainSync.size());
  resState.finalize(*this);
  vkAssert(vkEndCommandBuffer(impl));
  state = NoRecording;
  }

bool VCommandBuffer::isRecording() const {
  return state!=NoRecording;
  }

void VCommandBuffer::beginRenderPass(AbstractGraphicsApi::Fbo*   f,
                                     AbstractGraphicsApi::Pass*  p,
                                     uint32_t width,uint32_t height) {
  VFramebuffer& fbo =*reinterpret_cast<VFramebuffer*>(f);
  VRenderPass&  pass=*reinterpret_cast<VRenderPass*>(p);

  for(size_t i=0;i<fbo.attach.size();++i) {
    const bool preserve = pass.isAttachPreserved(i);
    resState.setLayout(fbo.attach[i],fbo.attach[i].renderLayout(),preserve);
    if(fbo.attach[i].sw!=nullptr)
      addDependency(*fbo.attach[i].sw,fbo.attach[i].id);
    }

  isInCompute = false;
  resState.flushLayout(*this);

  if(fbo.rp.handler->attCount!=pass.attCount)
    throw IncompleteFboException();

  auto& rp = pass.instance(*fbo.rp.handler);
  curFbo = &fbo;
  curRp  = &pass;

  VkRenderPassBeginInfo renderPassInfo = {};
  renderPassInfo.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  renderPassInfo.renderPass        = rp.impl;
  renderPassInfo.framebuffer       = fbo.impl;
  renderPassInfo.renderArea.offset = {0, 0};
  renderPassInfo.renderArea.extent = {width,height};

  renderPassInfo.clearValueCount   = pass.attCount;
  renderPassInfo.pClearValues      = rp.clear.get();

  vkCmdBeginRenderPass(impl, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

  state = RenderPass;

  // setup dynamic state
  // https://www.khronos.org/registry/vulkan/specs/1.1-extensions/html/vkspec.html#pipelines-dynamic-state
  setViewport(Rect(0,0,int32_t(width),int32_t(height)));

  VkRect2D scissor = {};
  scissor.offset = {0, 0};
  scissor.extent = {width,height};
  vkCmdSetScissor (impl,0,1,&scissor);
  }

void VCommandBuffer::endRenderPass() {
  for(size_t i=0;i<curFbo->attach.size();++i) {
    if(!curRp->isResultPreserved(i))
      continue;
    resState.setLayout(curFbo->attach[i],curFbo->attach[i].defaultLayout(),true);
    }
  vkCmdEndRenderPass(impl);
  state  = NoPass;
  curFbo = nullptr;
  curRp  = nullptr;
  }

void VCommandBuffer::setPipeline(AbstractGraphicsApi::Pipeline& p) {
  if(curFbo==nullptr)
    throw std::system_error(Tempest::GraphicsErrc::DrawCallWithoutFbo);
  VPipeline&           px = reinterpret_cast<VPipeline&>(p);
  VFramebufferLayout*  l  = reinterpret_cast<VFramebufferLayout*>(curFbo->rp.handler);
  auto& v = px.instance(*l);
  vkCmdBindPipeline(impl,VK_PIPELINE_BIND_POINT_GRAPHICS,v.val);
  ssboBarriers = px.ssboBarriers;
  }

void VCommandBuffer::setBytes(AbstractGraphicsApi::Pipeline& p, const void* data, size_t size) {
  VPipeline&        px=reinterpret_cast<VPipeline&>(p);
  vkCmdPushConstants(impl, px.pipelineLayout, px.pushStageFlags, 0, uint32_t(size), data);
  }

void VCommandBuffer::setUniforms(AbstractGraphicsApi::Pipeline &p, AbstractGraphicsApi::Desc &u) {
  VPipeline&        px=reinterpret_cast<VPipeline&>(p);
  VDescriptorArray& ux=reinterpret_cast<VDescriptorArray&>(u);
  curUniforms = &ux;
  vkCmdBindDescriptorSets(impl,VK_PIPELINE_BIND_POINT_GRAPHICS,
                          px.pipelineLayout,0,
                          1,&ux.desc,
                          0,nullptr);
  }

void VCommandBuffer::setComputePipeline(AbstractGraphicsApi::CompPipeline& p) {
  if(curFbo!=nullptr)
    throw std::system_error(Tempest::GraphicsErrc::ComputeCallInRenderPass);
  if(!isInCompute) {
    resState.flushLayout(*this);
    isInCompute = true;
    }
  VCompPipeline& px = reinterpret_cast<VCompPipeline&>(p);
  vkCmdBindPipeline(impl,VK_PIPELINE_BIND_POINT_COMPUTE,px.impl);
  ssboBarriers = px.ssboBarriers;
  }

void VCommandBuffer::setBytes(AbstractGraphicsApi::CompPipeline& p, const void* data, size_t size) {
  VCompPipeline& px=reinterpret_cast<VCompPipeline&>(p);
  vkCmdPushConstants(impl, px.pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, uint32_t(size), data);
  }

void VCommandBuffer::setUniforms(AbstractGraphicsApi::CompPipeline& p, AbstractGraphicsApi::Desc& u) {
  VCompPipeline&    px=reinterpret_cast<VCompPipeline&>(p);
  VDescriptorArray& ux=reinterpret_cast<VDescriptorArray&>(u);
  curUniforms = &ux;
  vkCmdBindDescriptorSets(impl,VK_PIPELINE_BIND_POINT_COMPUTE,
                          px.pipelineLayout,0,
                          1,&ux.desc,
                          0,nullptr);
  }

void VCommandBuffer::draw(const AbstractGraphicsApi::Buffer& ivbo, size_t offset, size_t size, size_t firstInstance, size_t instanceCount) {
  if(T_UNLIKELY(ssboBarriers)) {
    curUniforms->ssboBarriers(resState);
    resState.flushSSBO(*this);
    }
  const VBuffer& vbo=reinterpret_cast<const VBuffer&>(ivbo);
  if(curVbo!=vbo.impl) {
    VkBuffer     buffers[1] = {vbo.impl};
    VkDeviceSize offsets[1] = {0};
    vkCmdBindVertexBuffers(impl, 0, 1, buffers, offsets );
    curVbo = vbo.impl;
    }
  vkCmdDraw(impl, uint32_t(size), uint32_t(instanceCount), uint32_t(offset), uint32_t(firstInstance));
  }

void VCommandBuffer::drawIndexed(const AbstractGraphicsApi::Buffer& ivbo, const AbstractGraphicsApi::Buffer& iibo, Detail::IndexClass cls,
                                 size_t ioffset, size_t isize, size_t voffset, size_t firstInstance, size_t instanceCount) {
  if(T_UNLIKELY(ssboBarriers)) {
    curUniforms->ssboBarriers(resState);
    resState.flushSSBO(*this);
    }
  static const VkIndexType type[]={
    VK_INDEX_TYPE_UINT16,
    VK_INDEX_TYPE_UINT32
    };

  const VBuffer& vbo = reinterpret_cast<const VBuffer&>(ivbo);
  const VBuffer& ibo = reinterpret_cast<const VBuffer&>(iibo);
  if(curVbo!=vbo.impl) {
    VkBuffer     buffers[1] = {vbo.impl};
    VkDeviceSize offsets[1] = {0};
    vkCmdBindVertexBuffers(impl, 0, 1, buffers, offsets );
    curVbo = vbo.impl;
    }
  vkCmdBindIndexBuffer(impl, ibo.impl, 0, type[uint32_t(cls)]);
  vkCmdDrawIndexed    (impl, uint32_t(isize), uint32_t(instanceCount), uint32_t(ioffset), int32_t(voffset), uint32_t(firstInstance));
  }

void VCommandBuffer::dispatch(size_t x, size_t y, size_t z) {
  if(T_UNLIKELY(ssboBarriers)) {
    curUniforms->ssboBarriers(resState);
    resState.flushSSBO(*this);
    }
  vkCmdDispatch(impl,uint32_t(x),uint32_t(y),uint32_t(z));
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

void VCommandBuffer::copy(AbstractGraphicsApi::Buffer& dstBuf, size_t offsetDest, const AbstractGraphicsApi::Buffer &srcBuf, size_t offsetSrc, size_t size) {
  auto& src = reinterpret_cast<const VBuffer&>(srcBuf);
  auto& dst = reinterpret_cast<VBuffer&>(dstBuf);

  VkBufferCopy copyRegion = {};
  copyRegion.dstOffset = offsetDest;
  copyRegion.srcOffset = offsetSrc;
  copyRegion.size      = size;
  vkCmdCopyBuffer(impl, src.impl, dst.impl, 1, &copyRegion);
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

void VCommandBuffer::implCopy(AbstractGraphicsApi::Buffer& dstBuf, size_t width, size_t height, size_t mip,
                              const AbstractGraphicsApi::Texture& srcTex, size_t offset) {
  auto& src = reinterpret_cast<const VTexture&>(srcTex);
  auto& dst = reinterpret_cast<VBuffer&>(dstBuf);

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

  vkCmdCopyImageToBuffer(impl, src.impl, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dst.impl, 1, &region);
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

void Tempest::Detail::VCommandBuffer::copy(AbstractGraphicsApi::Buffer& dest, TextureLayout defLayout,
                                           uint32_t width, uint32_t height, uint32_t mip,
                                           AbstractGraphicsApi::Texture& src, size_t offset) {
  if(curFbo!=nullptr)
    throw std::system_error(Tempest::GraphicsErrc::ComputeCallInRenderPass);
  //resState.setLayout(src,TextureLayout::TransferSrc,true); // TODO: more advanced layout tracker
  resState.flushLayout(*this);

  changeLayout(src,defLayout,TextureLayout::TransferSrc,mip);
  implCopy(dest,width,height,mip,src,offset);
  changeLayout(src,TextureLayout::TransferSrc,defLayout,mip);
  }

void VCommandBuffer::changeLayout(AbstractGraphicsApi::Buffer& buf, BufferLayout prev, BufferLayout next) {
  VkBufferMemoryBarrier barrier = {};
  barrier.sType                 = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;

  barrier.srcQueueFamilyIndex   = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex   = VK_QUEUE_FAMILY_IGNORED;
  barrier.buffer                = reinterpret_cast<VBuffer&>(buf).impl;
  barrier.offset                = 0;
  barrier.size                  = VK_WHOLE_SIZE;

  VkPipelineStageFlags srcStage = 0;
  VkPipelineStageFlags dstStage = 0;

  bool hadWrite = false;
  if(prev==BufferLayout::ComputeWrite || prev==BufferLayout::ComputeReadWrite) {
    hadWrite = true;
    }

  switch(next) {
    case BufferLayout::Undefined:
      break;
    case BufferLayout::ComputeRead:
      if(hadWrite) {
        // Read-after-Write
        barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        srcStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        dstStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        } else {
        // Read-after-Read
        return;
        }
      break;
    case BufferLayout::ComputeReadWrite:
      if(hadWrite) {
        // Read-after-Write
        barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        srcStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        dstStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        } else {
        // Write-after-Read
        barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;

        srcStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        dstStage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        }
      break;
    default:
      // unused for now
      throw DeviceLostException();
    }

  vkCmdPipelineBarrier(
        impl,
        srcStage, dstStage,
        0,
        0, nullptr,
        1, &barrier,
        0, nullptr);
  }

void VCommandBuffer::changeLayout(AbstractGraphicsApi::Attach& att, TextureLayout prev, TextureLayout next, bool byRegion) {
  auto&    img          = reinterpret_cast<VFramebuffer::Attach&>(att);
  VkImage  nativeImg    = VK_NULL_HANDLE;
  VkFormat nativeFormat = VK_FORMAT_UNDEFINED;
  if(img.sw!=nullptr) {
    nativeImg    = img.sw->images[img.id];
    nativeFormat = img.sw->format();
    } else {
    nativeImg    = img.tex->impl;
    nativeFormat = img.tex->format;
    }

  auto p = (prev==TextureLayout::Undefined ? att.defaultLayout() : prev);
  implChangeLayout(nativeImg, nativeFormat,
                   Detail::nativeFormat(p), Detail::nativeFormat(next),
                   prev==TextureLayout::Undefined, 0, VK_REMAINING_MIP_LEVELS, byRegion);
  }

void VCommandBuffer::changeLayout(AbstractGraphicsApi::Texture& t,
                                  TextureLayout prev, TextureLayout next, uint32_t mipId) {
  auto&    vt       = reinterpret_cast<VTexture&>(t);
  auto     p        = (prev==TextureLayout::Undefined ? TextureLayout::Sampler : prev);
  uint32_t mipBase  = (mipId==uint32_t(-1) ? 0         : mipId);
  uint32_t mipCount = (mipId==uint32_t(-1) ? vt.mipCnt : 1);

  implChangeLayout(vt.impl,vt.format,
                   Detail::nativeFormat(p), Detail::nativeFormat(next),
                   prev==TextureLayout::Undefined, mipBase, mipCount, false);
  }

void VCommandBuffer::generateMipmap(AbstractGraphicsApi::Texture& img,
                                    TextureLayout defLayout,
                                    uint32_t texWidth, uint32_t texHeight, uint32_t mipLevels) {
  if(curFbo!=nullptr)
    throw std::system_error(Tempest::GraphicsErrc::ComputeCallInRenderPass);
  resState.flushLayout(*this);

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

  if(defLayout!=TextureLayout::TransferDest) {
    changeLayout(img,defLayout,TextureLayout::TransferDest,uint32_t(-1));
    }

  for(uint32_t i=1; i<mipLevels; ++i) {
    const int mw = (w==1 ? 1 : w/2);
    const int mh = (h==1 ? 1 : h/2);

    changeLayout(img,TextureLayout::TransferDest,TextureLayout::TransferSrc,i-1);
    blit(img,  w, h, i-1,
         img, mw,mh, i);
    changeLayout(img,TextureLayout::TransferSrc, TextureLayout::Sampler,    i-1);

    w = mw;
    h = mh;
    }
  changeLayout(img,TextureLayout::TransferDest, TextureLayout::Sampler, mipLevels-1);
  }

static VkPipelineStageFlags accessToStage(const VkAccessFlags a, const VulkanInstance::VkProp& prop) {
  VkPipelineStageFlags ret = 0;

  if(a&(VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_UNIFORM_READ_BIT)) {
    ret |= (VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT  |
            VK_PIPELINE_STAGE_VERTEX_SHADER_BIT   |
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT);
    if(prop.geometryShader)
      ret |= VK_PIPELINE_STAGE_GEOMETRY_SHADER_BIT;
    if(prop.tesselationShader)
      ret |= VK_PIPELINE_STAGE_TESSELLATION_EVALUATION_SHADER_BIT |
             VK_PIPELINE_STAGE_TESSELLATION_CONTROL_SHADER_BIT;

    /* TODO: ray tracing feature
    ret |= (VK_PIPELINE_STAGE_RAY_TRACING_SHADER_BIT_KHR      |
            VK_PIPELINE_STAGE_MESH_SHADER_BIT_NV |
            VK_PIPELINE_STAGE_TASK_SHADER_BIT_NV);
    */
    }

  if(a&(VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT)) {
    ret |= VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    }

  if(a&(VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT)) {
    ret |= VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    ret |= VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    }

  if(a&(VK_ACCESS_TRANSFER_READ_BIT|VK_ACCESS_TRANSFER_WRITE_BIT)) {
    ret |= VK_PIPELINE_STAGE_TRANSFER_BIT;
    }

  if(a&(VK_ACCESS_HOST_READ_BIT|VK_ACCESS_HOST_WRITE_BIT)) {
    ret |= VK_PIPELINE_STAGE_HOST_BIT;
    }

  if(a&(VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT)) {
    ret |= VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
    }

  return ret;
  }

static VkAccessFlags layoutToAccess(VkImageLayout lay) {
  switch(lay) {
    case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
      return VK_ACCESS_TRANSFER_READ_BIT;
    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
      return VK_ACCESS_TRANSFER_WRITE_BIT;
    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
    case VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL:
    case VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL:
    case VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL:
    case VK_IMAGE_LAYOUT_STENCIL_READ_ONLY_OPTIMAL:
      return VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT|VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
      return VK_ACCESS_COLOR_ATTACHMENT_READ_BIT|VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
      return VK_ACCESS_SHADER_READ_BIT;
    case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
      // can be BOTTOM_OF_PIPE for dest-access
      return VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
    case VK_IMAGE_LAYOUT_GENERAL:
      return VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT;
    case VK_IMAGE_LAYOUT_UNDEFINED:
    case VK_IMAGE_LAYOUT_MAX_ENUM:
      throw std::invalid_argument("invalid layout transition!");
    case VK_IMAGE_LAYOUT_PREINITIALIZED:
    case VK_IMAGE_LAYOUT_SHARED_PRESENT_KHR:
    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:
    case VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL:
    case VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL:
    //case VK_IMAGE_LAYOUT_SHADING_RATE_OPTIMAL_NV:
    //case VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT:
      //TODO
      throw std::invalid_argument("unimplemented layout transition!");
    }
  throw std::invalid_argument("unimplemented layout transition!");
  }

void VCommandBuffer::implChangeLayout(VkImage dest, VkFormat imageFormat,
                                      VkImageLayout oldLayout, VkImageLayout newLayout,
                                      bool discardOld,
                                      uint32_t mipBase, uint32_t mipCount,
                                      bool byRegion) {
  VkImageMemoryBarrier barrier = {};
  barrier.sType                = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.oldLayout            = oldLayout;
  barrier.newLayout            = newLayout;
  barrier.srcQueueFamilyIndex  = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex  = VK_QUEUE_FAMILY_IGNORED;
  barrier.image                = dest;

  if(device.graphicsQueue->family!=device.presentQueue->family) {
    if(newLayout==VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) {
      barrier.srcQueueFamilyIndex  = device.graphicsQueue->family;
      barrier.dstQueueFamilyIndex  = device.presentQueue->family;
      }
    else if(oldLayout==VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) {
      barrier.srcQueueFamilyIndex  = device.presentQueue->family;
      barrier.dstQueueFamilyIndex  = device.graphicsQueue->family;
      }
    }

  barrier.subresourceRange.baseMipLevel   = mipBase;
  barrier.subresourceRange.levelCount     = mipCount;
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount     = VK_REMAINING_ARRAY_LAYERS;

  if(imageFormat==VK_FORMAT_D24_UNORM_S8_UINT)
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT; else
  if(Detail::nativeIsDepthFormat(imageFormat))
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT; else
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

  VkAccessFlags srcAccessMask = layoutToAccess(oldLayout);
  VkAccessFlags dstAccessMask = layoutToAccess(newLayout);

  VkPipelineStageFlags srcStage = accessToStage(srcAccessMask,device.props);
  VkPipelineStageFlags dstStage = accessToStage(dstAccessMask,device.props);

  barrier.srcAccessMask = srcAccessMask;
  barrier.dstAccessMask = dstAccessMask;

  VkDependencyFlags depFlg=0;
  if(byRegion)
    depFlg = VK_DEPENDENCY_BY_REGION_BIT;
  if(discardOld)
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;

  vkCmdPipelineBarrier(
      impl,
      srcStage, dstStage,
      depFlg,
      0, nullptr,
      0, nullptr,
      1, &barrier
        );
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
  swapchainSync.push_back(sc);
  }

#endif
