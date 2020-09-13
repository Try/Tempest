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

struct VCommandBuffer::ImgState {
  VkImage       img;
  VkFormat      frm;
  VkImageLayout lay;
  VkImageLayout last;
  bool          outdated;
  };

VCommandBuffer::VCommandBuffer(VDevice& device, VkCommandPoolCreateFlags flags)
  :device(device), pool(device,flags) {
  VkCommandBufferAllocateInfo allocInfo = {};
  allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocInfo.commandPool        = pool.impl;
  allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocInfo.commandBufferCount = 1;

  vkAssert(vkAllocateCommandBuffers(device.device,&allocInfo,&impl));
  }

VCommandBuffer::~VCommandBuffer() {
  vkFreeCommandBuffers(device.device,pool.impl,1,&impl);
  }

void VCommandBuffer::reset() {
  vkResetCommandBuffer(impl,VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);
  }

void VCommandBuffer::begin() {
  begin(0);
  }

void VCommandBuffer::begin(VkCommandBufferUsageFlags flg) {
  state = NoPass;

  VkCommandBufferBeginInfo beginInfo = {};
  beginInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags            = flg;
  beginInfo.pInheritanceInfo = nullptr;

  vkAssert(vkBeginCommandBuffer(impl,&beginInfo));
  }

void VCommandBuffer::end() {
  if(state==RenderPass)
    endRenderPass();
  for(auto& i:imgState)
    i.outdated = true;
  flushLayout();
  imgState.clear();
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

  for(auto& i:imgState)
    i.outdated = true;

  for(size_t i=0;i<fbo.attach.size();++i) {
    VkFormat      frm = fbo.rp.handler->frm[i];
    VkImageLayout lay = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    const bool preserve = pass.isAttachPreserved(i);
    if(Detail::nativeIsDepthFormat(frm)) {
      lay = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
      setLayout(fbo.attach[i],frm,lay,preserve);
      } else {
      setLayout(fbo.attach[i],frm,lay,preserve);
      }
    }

  flushLayout();

  if(fbo.rp.handler->attCount!=pass.attCount)
    throw IncompleteFboException();

  auto& rp = pass.instance(*fbo.rp.handler);
  curFbo = fbo.rp;

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
  }

void VCommandBuffer::endRenderPass() {
  vkCmdEndRenderPass(impl);
  state = NoPass;
  }

void VCommandBuffer::setPipeline(AbstractGraphicsApi::Pipeline &p,uint32_t w,uint32_t h) {
  VPipeline&           px = reinterpret_cast<VPipeline&>(p);
  VFramebufferLayout*  l  = reinterpret_cast<VFramebufferLayout*>(curFbo.handler);
  auto& v = px.instance(*l,w,h);
  vkCmdBindPipeline(impl,VK_PIPELINE_BIND_POINT_GRAPHICS,v.val);
  }

void VCommandBuffer::setBytes(AbstractGraphicsApi::Pipeline& p, const void* data, size_t size) {
  VPipeline&        px=reinterpret_cast<VPipeline&>(p);
  vkCmdPushConstants(impl, px.pipelineLayout, px.pushStageFlags, 0, uint32_t(size), data);
  }

void VCommandBuffer::setUniforms(AbstractGraphicsApi::Pipeline &p, AbstractGraphicsApi::Desc &u) {
  VPipeline&        px=reinterpret_cast<VPipeline&>(p);
  VDescriptorArray& ux=reinterpret_cast<VDescriptorArray&>(u);
  vkCmdBindDescriptorSets(impl,VK_PIPELINE_BIND_POINT_GRAPHICS,
                          px.pipelineLayout,0,
                          1,&ux.desc,
                          0,nullptr);
  }

void VCommandBuffer::draw(size_t offset,size_t size) {
  vkCmdDraw(impl,uint32_t(size), 1, uint32_t(offset),0);
  }

void VCommandBuffer::drawIndexed(size_t ioffset, size_t isize, size_t voffset) {
  vkCmdDrawIndexed(impl,uint32_t(isize),1, uint32_t(ioffset), int32_t(voffset),0);
  }

void VCommandBuffer::setVbo(const Tempest::AbstractGraphicsApi::Buffer &b) {
  const VBuffer& vbo=reinterpret_cast<const VBuffer&>(b);

  std::initializer_list<VkBuffer>     buffers = {vbo.impl};
  std::initializer_list<VkDeviceSize> offsets = {0};
  vkCmdBindVertexBuffers(
        impl, 0, uint32_t(buffers.size()),
        buffers.begin(),
        offsets.begin() );
  }

void VCommandBuffer::setIbo(const AbstractGraphicsApi::Buffer& b,Detail::IndexClass cls) {
  static const VkIndexType type[]={
    VK_INDEX_TYPE_UINT16,
    VK_INDEX_TYPE_UINT32
    };

  const VBuffer& ibo=reinterpret_cast<const VBuffer&>(b);
  vkCmdBindIndexBuffer(impl,ibo.impl,0,type[uint32_t(cls)]);
  }

void VCommandBuffer::setViewport(const Tempest::Rect &r) {
  viewPort.x        = float(r.x);
  viewPort.y        = float(r.y);
  viewPort.width    = float(r.w);
  viewPort.height   = float(r.h);
  viewPort.minDepth = 0;
  viewPort.maxDepth = 1;

  vkCmdSetViewport(impl,0,1,&viewPort);
  }

void VCommandBuffer::setLayout(VFramebuffer::Attach& a, VkFormat frm, VkImageLayout lay, bool preserve) {
  ImgState* img;
  if(a.tex!=nullptr) {
    if(Detail::nativeIsDepthFormat(frm)) {
      img = &findImg(a.tex->impl,frm,VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,preserve);
      } else {
      img = &findImg(a.tex->impl,frm,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,preserve);
      }
    } else {
    img = &findImg(a.sw->images[a.id],frm,VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,preserve);
    }
  changeLayout(img->img,img->frm,img->lay,lay,VK_REMAINING_MIP_LEVELS,true);
  img->outdated = false;
  img->lay      = lay;
  }

VCommandBuffer::ImgState& VCommandBuffer::findImg(VkImage img, VkFormat frm, VkImageLayout last, bool preserve) {
  for(auto& i:imgState) {
    if(i.img==img)
      return i;
    }
  ImgState s={};
  s.img     = img;
  s.frm     = frm;
  s.lay     = preserve ? last : VK_IMAGE_LAYOUT_UNDEFINED;
  s.last    = last;
  imgState.push_back(s);
  return imgState.back();
  }

void VCommandBuffer::flushLayout() {
  for(auto& i:imgState) {
    if(!i.outdated)
      continue;
    if(Detail::nativeIsDepthFormat(i.frm))
      continue; // no readable depth for now
    changeLayout(i.img,i.frm,i.lay,i.last,VK_REMAINING_MIP_LEVELS,true);
    i.lay = i.last;
    i.outdated = false;
    }
  }

void VCommandBuffer::flush(const VBuffer &src, size_t size) {
  VkBufferMemoryBarrier barrier={};
  barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;

  barrier.srcAccessMask = 0;
  barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.buffer              =src.impl;
  barrier.offset              =0;
  barrier.size                =size;

  vkCmdPipelineBarrier(
      impl,
      VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
      0,
      0, nullptr,
      1, &barrier,
      0, nullptr
        );
  }

void VCommandBuffer::copy(VBuffer &dest, size_t offsetDest, const VBuffer &src,size_t offsetSrc,size_t size) {
  VkBufferCopy copyRegion = {};
  copyRegion.dstOffset = offsetDest;
  copyRegion.srcOffset = offsetSrc;
  copyRegion.size      = size;
  vkCmdCopyBuffer(impl, src.impl, dest.impl, 1, &copyRegion);
  }

void VCommandBuffer::copy(VTexture &dest, size_t width, size_t height, size_t mip, const VBuffer &src, size_t offset) {
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

  vkCmdCopyBufferToImage(impl, src.impl, dest.impl, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
  }

void VCommandBuffer::copy(VBuffer &dest, size_t width, size_t height, size_t mip, const VTexture &src, size_t offset) {
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

  vkCmdCopyImageToBuffer(impl, src.impl, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, dest.impl, 1, &region);
  }

void VCommandBuffer::changeLayout(AbstractGraphicsApi::Swapchain& s, uint32_t id,
                                  TextureFormat f,
                                  TextureLayout prev, TextureLayout next) {
  auto& vs = reinterpret_cast<VSwapchain&>(s);
  changeLayout(vs.images[id],Detail::nativeFormat(f),
               Detail::nativeFormat(prev), Detail::nativeFormat(next),
               VK_REMAINING_MIP_LEVELS, false);
  }

void VCommandBuffer::changeLayout(Tempest::AbstractGraphicsApi::Texture &t,
                                  Tempest::TextureFormat f,
                                  Tempest::TextureLayout prev, Tempest::TextureLayout next) {
  auto& vt = reinterpret_cast<VTexture&>(t);
  changeLayout(vt.impl,Detail::nativeFormat(f),
               Detail::nativeFormat(prev), Detail::nativeFormat(next), VK_REMAINING_MIP_LEVELS,false);
  }

void VCommandBuffer::changeLayout(AbstractGraphicsApi::Texture& t, TextureFormat f,
                                  TextureLayout prev, TextureLayout next, uint32_t mipCnt) {
  auto& vt = reinterpret_cast<VTexture&>(t);
  changeLayout(vt.impl,Detail::nativeFormat(f),
               Detail::nativeFormat(prev), Detail::nativeFormat(next), mipCnt, false);
  }

void VCommandBuffer::changeLayout(VkImage dest, VkFormat imageFormat,
                                  VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipCount,
                                  bool byRegion) {
  VkImageMemoryBarrier barrier = {};
  barrier.sType                = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.oldLayout            = oldLayout;
  barrier.newLayout            = newLayout;
  barrier.srcQueueFamilyIndex  = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex  = VK_QUEUE_FAMILY_IGNORED;
  barrier.image                = dest;

  barrier.subresourceRange.baseMipLevel   = 0;
  barrier.subresourceRange.levelCount     = mipCount;
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount     = 1;

  if(imageFormat==VK_FORMAT_D24_UNORM_S8_UINT)
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT; else
  if(Detail::nativeIsDepthFormat(imageFormat))
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT; else
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

  VkPipelineStageFlags sourceStage   = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
  VkPipelineStageFlags destStage     = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
  VkAccessFlags        srcAccessMask = 0;
  VkAccessFlags        dstAccessMask = 0;

  switch(oldLayout) {
    case VK_IMAGE_LAYOUT_UNDEFINED:
      srcAccessMask = 0;
      sourceStage   = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
      break;
    case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
      srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
      sourceStage   = VK_PIPELINE_STAGE_TRANSFER_BIT;
      break;
    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
      srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
      sourceStage   = VK_PIPELINE_STAGE_TRANSFER_BIT;
      break;
    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
    case VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL:
    case VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL:
    case VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL:
    case VK_IMAGE_LAYOUT_STENCIL_READ_ONLY_OPTIMAL:
      srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT|VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
      sourceStage   = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
      break;
    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
      srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT|VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
      //srcAccessMask |= VK_ACCESS_COLOR_ATTACHMENT_READ_NONCOHERENT_BIT_EXT;
      sourceStage   = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
      break;
    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
      srcAccessMask = 0;
      sourceStage   = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
      break;
    case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
      srcAccessMask = 0;
      sourceStage   = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
      break;
    case VK_IMAGE_LAYOUT_GENERAL:
    case VK_IMAGE_LAYOUT_PREINITIALIZED:
    case VK_IMAGE_LAYOUT_SHARED_PRESENT_KHR:
    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:
    case VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL:
    case VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL:
    //case VK_IMAGE_LAYOUT_SHADING_RATE_OPTIMAL_NV:
    //case VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT:
      //TODO
      throw std::invalid_argument("unimplemented layout transition!");
    case VK_IMAGE_LAYOUT_MAX_ENUM:
      break;
    }

  switch(newLayout) {
    case VK_IMAGE_LAYOUT_UNDEFINED:
      dstAccessMask = 0;
      destStage     = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
      break;
    case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
      dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
      destStage     = VK_PIPELINE_STAGE_TRANSFER_BIT;
      break;
    case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
      dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
      destStage     = VK_PIPELINE_STAGE_TRANSFER_BIT;
      break;
    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
    case VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL:
    case VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL:
    case VK_IMAGE_LAYOUT_STENCIL_ATTACHMENT_OPTIMAL:
    case VK_IMAGE_LAYOUT_STENCIL_READ_ONLY_OPTIMAL:
      dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT|VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
      destStage     = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
      break;
    case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
      dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT|VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
      destStage     = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
      break;
    case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
      dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
      destStage     = VK_PIPELINE_STAGE_VERTEX_SHADER_BIT;
      break;
    case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
      dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT|VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
      destStage     = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
      break;
    case VK_IMAGE_LAYOUT_GENERAL:
    case VK_IMAGE_LAYOUT_PREINITIALIZED:
    case VK_IMAGE_LAYOUT_SHARED_PRESENT_KHR:
    case VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL:
    case VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_STENCIL_ATTACHMENT_OPTIMAL:
    case VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_STENCIL_READ_ONLY_OPTIMAL:
    //case VK_IMAGE_LAYOUT_SHADING_RATE_OPTIMAL_NV:
    //case VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT:
      //TODO
      throw std::invalid_argument("unimplemented layout transition!");
    case VK_IMAGE_LAYOUT_MAX_ENUM:
      break;
    }

  barrier.srcAccessMask = srcAccessMask;
  barrier.dstAccessMask = dstAccessMask;

  VkDependencyFlags depFlg=0;
  if(byRegion)
    depFlg = VK_DEPENDENCY_BY_REGION_BIT;

  vkCmdPipelineBarrier(
      impl,
      sourceStage, destStage,
      depFlg,
      0, nullptr,
      0, nullptr,
      1, &barrier
      );
  }

void VCommandBuffer::generateMipmap(VTexture& image, TextureFormat frm,
                                    uint32_t texWidth, uint32_t texHeight, uint32_t mipLevels) {
  VkFormat imageFormat = Detail::nativeFormat(frm);
  // Check if image format supports linear blitting
  VkFormatProperties formatProperties;
  vkGetPhysicalDeviceFormatProperties(device.physicalDevice, imageFormat, &formatProperties);

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
