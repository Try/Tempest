#include "vcommandbuffer.h"

#include "vdevice.h"
#include "vcommandpool.h"
#include "vframebuffer.h"
#include "vrenderpass.h"
#include "vpipeline.h"
#include "vbuffer.h"
#include "vimage.h"
#include "vdescriptorarray.h"
#include "vswapchain.h"
#include "vtexture.h"

using namespace Tempest::Detail;

VCommandBuffer::VCommandBuffer(VDevice& device, VCommandPool& pool, CmdType secondary)
  :device(device.device), pool(pool.impl) {
  VkCommandBufferAllocateInfo allocInfo = {};
  allocInfo.sType             =VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocInfo.commandPool       =pool.impl;
  allocInfo.level             =(secondary==CmdType::Secondary) ? VK_COMMAND_BUFFER_LEVEL_SECONDARY : VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocInfo.commandBufferCount=1;

  vkAssert(vkAllocateCommandBuffers(device.device,&allocInfo,&impl));
  }

VCommandBuffer::VCommandBuffer(VCommandBuffer &&other) {
  std::swap(device,other.device);
  std::swap(pool,other.pool);
  std::swap(impl,other.impl);
  }

VCommandBuffer::~VCommandBuffer() {
  if(device==nullptr || impl==nullptr)
    return;
  vkFreeCommandBuffers(device,pool,1,&impl);
  }

void VCommandBuffer::operator=(VCommandBuffer &&other) {
  std::swap(device,other.device);
  std::swap(pool,other.pool);
  std::swap(impl,other.impl);
  }

void VCommandBuffer::reset() {
  vkResetCommandBuffer(impl,VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);
  }

void VCommandBuffer::begin(Usage usageFlags) {
  VkCommandBufferBeginInfo beginInfo = {};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags = usageFlags; //VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
  //beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

  vkAssert(vkBeginCommandBuffer(impl,&beginInfo));
  }

void VCommandBuffer::begin(VCommandBuffer::Usage usageFlags, VFramebuffer &fbo, VRenderPass &rpass) {
  VkCommandBufferInheritanceInfo inheritanceInfo={};
  inheritanceInfo.sType       = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
  inheritanceInfo.framebuffer = fbo.impl;
  inheritanceInfo.renderPass  = rpass.impl;

  VkCommandBufferBeginInfo beginInfo = {};
  beginInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags            = usageFlags;
  beginInfo.pInheritanceInfo = &inheritanceInfo;

  vkAssert(vkBeginCommandBuffer(impl,&beginInfo));
  }

void VCommandBuffer::begin() {
  begin(Usage(SIMULTANEOUS_USE_BIT));
  }

void VCommandBuffer::begin(Tempest::AbstractGraphicsApi::Fbo *f, Tempest::AbstractGraphicsApi::Pass *p) {
  VFramebuffer* fbo =reinterpret_cast<VFramebuffer*>(f);
  VRenderPass*  pass=reinterpret_cast<VRenderPass*>(p);
  begin(Usage(SIMULTANEOUS_USE_BIT|RENDER_PASS_CONTINUE_BIT),*fbo,*pass);
  }

void VCommandBuffer::end() {
  vkAssert(vkEndCommandBuffer(impl));
  }

void VCommandBuffer::beginRenderPass(AbstractGraphicsApi::Fbo*   f,
                                     AbstractGraphicsApi::Pass*  p,
                                     uint32_t width,uint32_t height) {
  VFramebuffer* fbo =reinterpret_cast<VFramebuffer*>(f);
  VRenderPass*  pass=reinterpret_cast<VRenderPass*>(p);

  VkRenderPassBeginInfo renderPassInfo = {};
  renderPassInfo.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  renderPassInfo.renderPass        = pass->impl;
  renderPassInfo.framebuffer       = fbo->impl;
  renderPassInfo.renderArea.offset = {0, 0};
  renderPassInfo.renderArea.extent = {width,height};

  VkClearValue clear[2]={};
  clear[0].color.float32[0]=pass->color.r();
  clear[0].color.float32[1]=pass->color.g();
  clear[0].color.float32[2]=pass->color.b();
  clear[0].color.float32[3]=pass->color.a();

  clear[1].depthStencil.depth=pass->zclear;

  renderPassInfo.clearValueCount = pass->attachCount;
  renderPassInfo.pClearValues    = clear;

  vkCmdBeginRenderPass(impl, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
  }

void VCommandBuffer::endRenderPass() {
  vkCmdEndRenderPass(impl);
  }

void VCommandBuffer::clear(AbstractGraphicsApi::Image& image,
                           float r, float g, float b, float a) {
  VImage&     img=reinterpret_cast<VImage&>(image);
  uint32_t presentQueueFamily = img.presentQueueFamily;

  VkImageSubresourceRange subResourceRange = {};
  subResourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
  subResourceRange.baseMipLevel   = 0;
  subResourceRange.levelCount     = 1;
  subResourceRange.baseArrayLayer = 0;
  subResourceRange.layerCount     = 1;

  VkImageMemoryBarrier presentToClearBarrier = {};
  presentToClearBarrier.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  presentToClearBarrier.srcAccessMask       = VK_ACCESS_MEMORY_READ_BIT;
  presentToClearBarrier.dstAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT;
  presentToClearBarrier.oldLayout           = VK_IMAGE_LAYOUT_UNDEFINED;
  presentToClearBarrier.newLayout           = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  presentToClearBarrier.srcQueueFamilyIndex = presentQueueFamily;
  presentToClearBarrier.dstQueueFamilyIndex = presentQueueFamily;
  presentToClearBarrier.image               = img.impl;
  presentToClearBarrier.subresourceRange    = subResourceRange;

  VkImageMemoryBarrier clearToPresentBarrier = {};
  clearToPresentBarrier.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  clearToPresentBarrier.srcAccessMask       = VK_ACCESS_TRANSFER_WRITE_BIT;
  clearToPresentBarrier.dstAccessMask       = VK_ACCESS_MEMORY_READ_BIT;
  clearToPresentBarrier.oldLayout           = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  clearToPresentBarrier.newLayout           = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
  clearToPresentBarrier.srcQueueFamilyIndex = presentQueueFamily;
  clearToPresentBarrier.dstQueueFamilyIndex = presentQueueFamily;
  clearToPresentBarrier.image               = img.impl;
  clearToPresentBarrier.subresourceRange    = subResourceRange;

  VkClearColorValue clearColor = { {r,g,b,a} };
  VkClearValue clearValue = {};
  clearValue.color = clearColor;

  vkCmdPipelineBarrier(impl, VK_PIPELINE_STAGE_TRANSFER_BIT,
                       VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &presentToClearBarrier);
  vkCmdClearColorImage(impl,img.impl,
                       VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,&clearColor,1,&subResourceRange);
  vkCmdPipelineBarrier(impl, VK_PIPELINE_STAGE_TRANSFER_BIT,
                       VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &clearToPresentBarrier);
  }

void VCommandBuffer::setPipeline(AbstractGraphicsApi::Pipeline &p) {
  VPipeline& px=reinterpret_cast<VPipeline&>(p);
  vkCmdBindPipeline(impl,VK_PIPELINE_BIND_POINT_GRAPHICS,px.graphicsPipeline);
  }

void VCommandBuffer::setUniforms(AbstractGraphicsApi::Pipeline &p, AbstractGraphicsApi::Desc &u, size_t offc, const uint32_t *offv) {
  VPipeline&        px=reinterpret_cast<VPipeline&>(p);
  VDescriptorArray& ux=reinterpret_cast<VDescriptorArray&>(u);
  vkCmdBindDescriptorSets(impl,VK_PIPELINE_BIND_POINT_GRAPHICS,
                          px.pipelineLayout,0,
                          1,ux.desc,
                          offc,offv);
  }

void VCommandBuffer::exec(const CommandBuffer &buf) {
  const VCommandBuffer& cmd=reinterpret_cast<const VCommandBuffer&>(buf);
  vkCmdExecuteCommands(impl,1,&cmd.impl);
  }

void VCommandBuffer::draw(size_t offset,size_t size) {
  vkCmdDraw(impl,size, 1, offset,0);
  }

void VCommandBuffer::drawIndexed(size_t ioffset, size_t isize, size_t voffset) {
  vkCmdDrawIndexed(impl,isize,1, ioffset, int32_t(voffset),0);
  }

void VCommandBuffer::setVbo(const Tempest::AbstractGraphicsApi::Buffer &b) {
  const VBuffer& vbo=reinterpret_cast<const VBuffer&>(b);

  std::initializer_list<VkBuffer>     buffers = {vbo.impl};
  std::initializer_list<VkDeviceSize> offsets = {0};
  vkCmdBindVertexBuffers(
        impl, 0, buffers.size(),
        buffers.begin(),
        offsets.begin() );
  }

void VCommandBuffer::setIbo(const AbstractGraphicsApi::Buffer *b,Detail::IndexClass cls) {
  if(b==nullptr)
    return;

  static const VkIndexType type[]={
    VK_INDEX_TYPE_UINT16,
    VK_INDEX_TYPE_UINT32
    };
  const VBuffer& ibo=reinterpret_cast<const VBuffer&>(*b);
  vkCmdBindIndexBuffer(impl,ibo.impl,0,type[uint32_t(cls)]);
  }

void VCommandBuffer::copy(VBuffer &dest, size_t offsetDest, const VBuffer &src,size_t offsetSrc,size_t size) {
  VkBufferCopy copyRegion = {};
  copyRegion.dstOffset = offsetDest;
  copyRegion.srcOffset = offsetSrc;
  copyRegion.size      = size;
  vkCmdCopyBuffer(impl, src.impl, dest.impl, 1, &copyRegion);
  }

void VCommandBuffer::copy(VTexture &dest, size_t width, size_t height, const VBuffer &src) {
  VkBufferImageCopy region = {};
  region.bufferOffset = 0;
  region.bufferRowLength = 0;
  region.bufferImageHeight = 0;
  region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  region.imageSubresource.mipLevel = 0;
  region.imageSubresource.baseArrayLayer = 0;
  region.imageSubresource.layerCount = 1;
  region.imageOffset = {0, 0, 0};
  region.imageExtent = {
      width,
      height,
      1
  };

  vkCmdCopyBufferToImage(impl, src.impl, dest.impl, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
  }

void VCommandBuffer::changeLayout(VTexture &dest,VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipCount) {
  VkImageMemoryBarrier barrier = {};
  barrier.sType               = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.oldLayout           = oldLayout;
  barrier.newLayout           = newLayout;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.image               = dest.impl;

  barrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
  barrier.subresourceRange.baseMipLevel   = 0;
  barrier.subresourceRange.levelCount     = mipCount;
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount     = 1;

  VkPipelineStageFlags sourceStage;
  VkPipelineStageFlags destinationStage;

  if(oldLayout==VK_IMAGE_LAYOUT_UNDEFINED && newLayout==VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

    sourceStage      = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else
  if(oldLayout==VK_IMAGE_LAYOUT_UNDEFINED && newLayout==VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT|VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

    sourceStage           = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    destinationStage      = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    } else
  if(oldLayout==VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout==VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    sourceStage           = VK_PIPELINE_STAGE_TRANSFER_BIT;
    destinationStage      = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else {
    //TODO
    throw std::invalid_argument("inimplemented layout transition!");
    }

  vkCmdPipelineBarrier(
      impl,
      sourceStage, destinationStage,
      0,
      0, nullptr,
      0, nullptr,
      1, &barrier
        );
  }

void VCommandBuffer::generateMipmap(VTexture& image,
                                    VkPhysicalDevice pdev, VkFormat imageFormat,
                                    uint32_t texWidth, uint32_t texHeight, uint32_t mipLevels) {
  // Check if image format supports linear blitting
  VkFormatProperties formatProperties;
  vkGetPhysicalDeviceFormatProperties(pdev, imageFormat, &formatProperties);

  if(!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT))
    throw std::runtime_error("texture image format does not support linear blitting!");

  VkImageMemoryBarrier barrier = {};
  barrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.image                           = image.impl;
  barrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
  barrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount     = 1;
  barrier.subresourceRange.levelCount     = 1;

  int32_t mipWidth  = int32_t(texWidth);
  int32_t mipHeight = int32_t(texHeight);

  for(uint32_t i=1; i<mipLevels; i++) {
    barrier.subresourceRange.baseMipLevel = i - 1;
    barrier.oldLayout     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout     = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

    vkCmdPipelineBarrier(impl,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
                         0, nullptr,
                         0, nullptr,
                         1, &barrier);

    VkImageBlit blit = {};
    blit.srcOffsets[0] = {0, 0, 0};
    blit.srcOffsets[1] = {mipWidth, mipHeight, 1};
    blit.srcSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    blit.srcSubresource.mipLevel       = i-1;
    blit.srcSubresource.baseArrayLayer = 0;
    blit.srcSubresource.layerCount     = 1;
    blit.dstOffsets[0] = {0, 0, 0};
    blit.dstOffsets[1] = { mipWidth==1 ? 1 : mipWidth/2, mipHeight==1 ? 1 : mipHeight/2, 1 };
    blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    blit.dstSubresource.mipLevel       = i;
    blit.dstSubresource.baseArrayLayer = 0;
    blit.dstSubresource.layerCount     = 1;

    vkCmdBlitImage(impl,
                   image.impl, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                   image.impl, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                   1, &blit,
                   VK_FILTER_LINEAR);

    barrier.oldLayout     = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.newLayout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(impl,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
                         0, nullptr,
                         0, nullptr,
                         1, &barrier);
    mipWidth  = mipWidth ==1 ? 1 : mipWidth /2;
    mipHeight = mipHeight==1 ? 1 : mipHeight/2;
    }

  barrier.subresourceRange.baseMipLevel = mipLevels - 1;
  barrier.oldLayout     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  barrier.newLayout     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

  vkCmdPipelineBarrier(impl,
                       VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
                       0, nullptr,
                       0, nullptr,
                       1, &barrier);
  }
