#include "vcommandbuffer.h"

#include "vdevice.h"
#include "vcommandpool.h"
#include "vframebuffer.h"
#include "vrenderpass.h"
#include "vpipeline.h"
#include "vbuffer.h"
#include "vimage.h"
#include "vswapchain.h"

using namespace Tempest::Detail;

VCommandBuffer::VCommandBuffer(VDevice& device,VCommandPool& pool)
  :device(device.device), pool(pool.impl) {
  VkCommandBufferAllocateInfo allocInfo = {};
  allocInfo.sType             =VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocInfo.commandPool       =pool.impl;
  allocInfo.level             =VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocInfo.commandBufferCount=1;

  if(vkAllocateCommandBuffers(device.device,&allocInfo,&impl)!=VK_SUCCESS)
    throw std::runtime_error("failed to allocate command buffers!");
  }

VCommandBuffer::VCommandBuffer(VCommandBuffer &&other) {
  std::swap(device,other.device);
  std::swap(pool,other.pool);
  std::swap(impl,other.impl);
  }

VCommandBuffer::~VCommandBuffer() {
  if(device==nullptr)
    return;
  vkDeviceWaitIdle(device);
  vkFreeCommandBuffers(device,pool,1,&impl);
  }

void VCommandBuffer::operator=(VCommandBuffer &&other) {
  std::swap(device,other.device);
  std::swap(pool,other.pool);
  std::swap(impl,other.impl);
  }

void VCommandBuffer::begin() {
  VkCommandBufferBeginInfo beginInfo = {};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;

  //vkResetCommandBuffer();
  if(vkBeginCommandBuffer(impl,&beginInfo)!=VK_SUCCESS)
    throw std::runtime_error("failed to begin recording command buffer!");
  }

void VCommandBuffer::end() {
  if(vkEndCommandBuffer(impl)!=VK_SUCCESS)
    throw std::runtime_error("failed to record command buffer!");
  }

void VCommandBuffer::beginRenderPass(AbstractGraphicsApi::Fbo*   f,
                                     AbstractGraphicsApi::Pass*  p,
                                     uint32_t width,uint32_t height) {
  VFramebuffer* fbo =reinterpret_cast<VFramebuffer*>(f);
  VRenderPass*  pass=reinterpret_cast<VRenderPass*>(p);

  VkRenderPassBeginInfo renderPassInfo = {};
  renderPassInfo.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  renderPassInfo.renderPass        = pass->renderPass;
  renderPassInfo.framebuffer       = fbo->impl;
  renderPassInfo.renderArea.offset = {0, 0};
  renderPassInfo.renderArea.extent = {width,height};

  VkClearValue clearColor = {{{0.0f, 0.0f, 1.0f, 1.0f}}};
  renderPassInfo.clearValueCount = 1;
  renderPassInfo.pClearValues    = &clearColor;

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

void VCommandBuffer::setPipeline(Tempest::AbstractGraphicsApi::Pipeline &p) {
  VPipeline& px=reinterpret_cast<VPipeline&>(p);
  vkCmdBindPipeline(impl,VK_PIPELINE_BIND_POINT_GRAPHICS,px.graphicsPipeline);
  }

void VCommandBuffer::draw(size_t size) {
  vkCmdDraw(impl,size,1, 0,0);
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
