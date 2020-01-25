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

struct VCommandBuffer::Secondarys final {
  Secondarys(VkDevice device, VkCommandPool pool)
    :device(device), pool(pool){
    }

  ~Secondarys() {
    if(secondaryChunks.empty())
      return;
    vkFreeCommandBuffers(device,pool,secondaryChunks.size(),secondaryChunks.data());
    }

  void flushSecondary(VkCommandBuffer mainCmd) {
    if(nonFlushed!=nullptr){
      vkAssert(vkEndCommandBuffer(nonFlushed));
      vkCmdExecuteCommands(mainCmd,1,&nonFlushed);
      nonFlushed = nullptr;
      }
    }

  void allocChunk(VkCommandBuffer mainCmd,const VFramebufferLayout& fboLay) {
    flushSecondary(mainCmd);

    VkCommandBuffer buffer = nullptr;
    VkCommandBufferAllocateInfo allocInfo = {};
    allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool        = pool;
    allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_SECONDARY;
    allocInfo.commandBufferCount = 1;

    vkAssert(vkAllocateCommandBuffers(device,&allocInfo,&buffer));
    try {
      VkCommandBufferInheritanceInfo inheritanceInfo={};
      inheritanceInfo.sType       = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
      inheritanceInfo.framebuffer = VK_NULL_HANDLE;
      inheritanceInfo.renderPass  = fboLay.impl;

      VkCommandBufferBeginInfo beginInfo = {};
      beginInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
      beginInfo.flags            = SIMULTANEOUS_USE_BIT|RENDER_PASS_CONTINUE_BIT;
      beginInfo.pInheritanceInfo = &inheritanceInfo;

      vkAssert(vkBeginCommandBuffer(buffer,&beginInfo));

      secondaryChunks.push_back(buffer);
      nonFlushed = buffer;
      }
    catch(...) {
      vkFreeCommandBuffers(device,pool,1,&buffer);
      throw;
      }
    }

  VkCommandBuffer back() {
    return secondaryChunks.back();
    }

  VkDevice                     device = nullptr;
  VkCommandPool                pool   = VK_NULL_HANDLE;

  VkCommandBuffer              nonFlushed=nullptr;
  std::vector<VkCommandBuffer> secondaryChunks;
  };

struct VCommandBuffer::BuildState final {
  Detail::DSharedPtr<VFramebufferLayout*> currentFbo;
  RpState                                 state=NoPass;
  Secondarys                              sec;

  BuildState(VkDevice device, VkCommandPool pool)
    :sec(device,pool){
    }

  void startPass(VkCommandBuffer impl, VFramebuffer* fbo, VRenderPass* pass,
                 uint32_t width,uint32_t height) {
    if(state!=NoPass)
      vkCmdEndRenderPass(impl);

    if(fbo->rp.handler->attCount!=pass->attCount)
      throw IncompleteFboException();

    auto& rp = pass->instance(*fbo->rp.handler);

    VkRenderPassBeginInfo renderPassInfo = {};
    renderPassInfo.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass        = rp.impl;
    renderPassInfo.framebuffer       = fbo->impl;
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = {width,height};

    renderPassInfo.clearValueCount   = pass->attCount;
    renderPassInfo.pClearValues      = rp.clear.get();
    vkCmdBeginRenderPass(impl, &renderPassInfo, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);

    currentFbo = fbo->rp;
    state = Pending;
    }

  void stopPass(VkCommandBuffer impl) {
    sec.flushSecondary(impl);
    vkCmdEndRenderPass(impl);
    state = NoPass;
    }

  VkCommandBuffer adjustRp(VkCommandBuffer impl,bool sec) {
    if(sec) {
      state = Secondary;
      return impl;
      }

    if(state!=Inline)
      this->sec.allocChunk(impl,*currentFbo.handler);

    state = Inline;
    return this->sec.back();
    }
  };

VCommandBuffer::VCommandBuffer(VDevice& device, VCommandPool& pool, VFramebufferLayout *fbo, CmdType secondary)
  :device(device.device), pool(pool.impl), fboLay(fbo) {
  VkCommandBufferAllocateInfo allocInfo = {};
  allocInfo.sType             =VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocInfo.commandPool       =pool.impl;
  allocInfo.level             =(secondary==CmdType::Secondary) ? VK_COMMAND_BUFFER_LEVEL_SECONDARY : VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocInfo.commandBufferCount=1;

  vkAssert(vkAllocateCommandBuffers(device.device,&allocInfo,&impl));
  }

VCommandBuffer::VCommandBuffer(VCommandBuffer &&other) {
  *this = std::move(other);
  }

VCommandBuffer::~VCommandBuffer() {
  if(device==nullptr || impl==nullptr)
    return;
  vkFreeCommandBuffers(device,pool,1,&impl);
  if(!chunks.empty())
    vkFreeCommandBuffers(device,pool,chunks.size(),chunks.data());
  }

VCommandBuffer& VCommandBuffer::operator=(VCommandBuffer &&other) {
  std::swap(impl,   other.impl);
  std::swap(device, other.device);
  std::swap(pool,   other.pool);
  std::swap(bstate, other.bstate);
  std::swap(chunks, other.chunks);
  std::swap(fboLay, other.fboLay);
  return *this;
  }

void VCommandBuffer::reset() {
  vkResetCommandBuffer(impl,VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);
  if(!chunks.empty()) {
    vkFreeCommandBuffers(device,pool,chunks.size(),chunks.data());
    chunks.clear();
    }
  bstate.reset();
  }

void VCommandBuffer::begin() {
  begin(SIMULTANEOUS_USE_BIT);
  }

void VCommandBuffer::begin(Usage usage) {
  VkCommandBufferUsageFlags      usageFlags = usage;
  VkCommandBufferInheritanceInfo inheritanceInfo={};

  if(isSecondary()){
    // secondary pass
    inheritanceInfo.sType       = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
    inheritanceInfo.framebuffer = VK_NULL_HANDLE;
    inheritanceInfo.renderPass  = fboLay.handler->impl;

    usageFlags = SIMULTANEOUS_USE_BIT|RENDER_PASS_CONTINUE_BIT;
    //bstate->currentFbo = fboLay;
    } else {
    // prime pass
    if(usage==SIMULTANEOUS_USE_BIT) {
      bstate.reset(new BuildState(device,pool));
      bstate->currentFbo = Detail::DSharedPtr<VFramebufferLayout*>{};
      }
    }

  VkCommandBufferBeginInfo beginInfo = {};
  beginInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags            = usageFlags;
  beginInfo.pInheritanceInfo = inheritanceInfo.renderPass!=VK_NULL_HANDLE ? &inheritanceInfo : nullptr;

  vkAssert(vkBeginCommandBuffer(impl,&beginInfo));
  }

void VCommandBuffer::end() {
  if(bstate!=nullptr) {
    std::swap(chunks,bstate->sec.secondaryChunks);
    bstate.reset();
    }
  vkAssert(vkEndCommandBuffer(impl));
  }

bool VCommandBuffer::isRecording() const {
  return bstate!=nullptr;
  }

void VCommandBuffer::beginRenderPass(AbstractGraphicsApi::Fbo*   f,
                                     AbstractGraphicsApi::Pass*  p,
                                     uint32_t width,uint32_t height) {
  VFramebuffer* fbo =reinterpret_cast<VFramebuffer*>(f);
  VRenderPass*  pass=reinterpret_cast<VRenderPass*>(p);

  bstate->startPass(impl,fbo,pass,width,height);

  // setup dynamic state
  // https://www.khronos.org/registry/vulkan/specs/1.1-extensions/html/vkspec.html#pipelines-dynamic-state
  setViewport(Rect(0,0,int32_t(width),int32_t(height)));
  }

void VCommandBuffer::endRenderPass() {
  bstate->stopPass(impl);
  }

void VCommandBuffer::setPipeline(AbstractGraphicsApi::Pipeline &p,uint32_t w,uint32_t h) {
  auto cmd = getBuffer(CmdRenderpass);

  VPipeline&           px = reinterpret_cast<VPipeline&>(p);
  VFramebufferLayout*  l;
  if(impl==cmd)
    l = reinterpret_cast<VFramebufferLayout*>(fboLay.handler); else
    l = reinterpret_cast<VFramebufferLayout*>(bstate->currentFbo.handler);
  auto& v = px.instance(*l,w,h);
  vkCmdBindPipeline(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,v.val);
  }

void VCommandBuffer::setUniforms(AbstractGraphicsApi::Pipeline &p, AbstractGraphicsApi::Desc &u, size_t offc, const uint32_t *offv) {
  auto cmd = getBuffer(CmdRenderpass);

  VPipeline&        px=reinterpret_cast<VPipeline&>(p);
  VDescriptorArray& ux=reinterpret_cast<VDescriptorArray&>(u);

  if(offc<=128) {
    uint32_t buf[128];
    implSetUniforms(cmd,px,ux,offc,offv,buf);
    } else {
    std::unique_ptr<uint32_t[]> buf(new uint32_t[offc]);
    implSetUniforms(cmd,px,ux,offc,offv,buf.get());
    }
  }

void VCommandBuffer::implSetUniforms(VkCommandBuffer cmd, VPipeline& px, VDescriptorArray& ux,
                                     size_t offc, const uint32_t* offv, uint32_t* buf) {
  for(size_t i=0;i<offc;++i)
    buf[i] = ux.multiplier[i]*offv[i];
  vkCmdBindDescriptorSets(cmd,VK_PIPELINE_BIND_POINT_GRAPHICS,
                          px.pipelineLayout,0,
                          1,&ux.desc,
                          offc,buf);
  }

void VCommandBuffer::setViewport(const Tempest::Rect &r) {
  auto cmd = getBuffer(CmdInline);

  VkViewport vk={};
  vk.x        = r.x;
  vk.y        = r.y;
  vk.width    = r.w;
  vk.height   = r.h;
  vk.minDepth = 0;
  vk.maxDepth = 1;
  vkCmdSetViewport(cmd,0,1,&vk);
  }

void VCommandBuffer::exec(const CommandBuffer &buf) {
  auto cmd = bstate->adjustRp(impl,true);
  const VCommandBuffer& ecmd=reinterpret_cast<const VCommandBuffer&>(buf);
  vkCmdExecuteCommands(cmd,1,&ecmd.impl);
  }

void VCommandBuffer::draw(size_t offset,size_t size) {
  auto cmd = getBuffer(CmdRenderpass);
  vkCmdDraw(cmd,size, 1, offset,0);
  }

void VCommandBuffer::drawIndexed(size_t ioffset, size_t isize, size_t voffset) {
  auto cmd = getBuffer(CmdRenderpass);
  vkCmdDrawIndexed(cmd,isize,1, ioffset, int32_t(voffset),0);
  }

void VCommandBuffer::setVbo(const Tempest::AbstractGraphicsApi::Buffer &b) {
  auto cmd = getBuffer(CmdRenderpass);
  const VBuffer& vbo=reinterpret_cast<const VBuffer&>(b);

  std::initializer_list<VkBuffer>     buffers = {vbo.impl};
  std::initializer_list<VkDeviceSize> offsets = {0};
  vkCmdBindVertexBuffers(
        cmd, 0, buffers.size(),
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

  auto cmd = getBuffer(CmdRenderpass);
  const VBuffer& ibo=reinterpret_cast<const VBuffer&>(*b);
  vkCmdBindIndexBuffer(cmd,ibo.impl,0,type[uint32_t(cls)]);
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
  static const VkImageLayout frm[]={
    VK_IMAGE_LAYOUT_UNDEFINED,
    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
    VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
    };

  auto& vs = reinterpret_cast<VSwapchain&>(s);
  changeLayout(vs.swapChainImages[id],Detail::nativeFormat(f),
               frm[uint8_t(prev)],frm[uint8_t(next)],VK_REMAINING_MIP_LEVELS);
  }

void VCommandBuffer::changeLayout(Tempest::AbstractGraphicsApi::Texture &t,
                                  Tempest::TextureFormat f,
                                  Tempest::TextureLayout prev, Tempest::TextureLayout next) {
  static const VkImageLayout frm[]={
    VK_IMAGE_LAYOUT_UNDEFINED,
    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
    VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
    };

  auto& vt = reinterpret_cast<VTexture&>(t);
  changeLayout(vt.impl,Detail::nativeFormat(f),
               frm[uint8_t(prev)],frm[uint8_t(next)],VK_REMAINING_MIP_LEVELS);
  }

void VCommandBuffer::changeLayout(VkImage dest, VkFormat imageFormat,
                                  VkImageLayout oldLayout, VkImageLayout newLayout, uint32_t mipCount) {
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
    case VK_IMAGE_LAYOUT_RANGE_SIZE:
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
    case VK_IMAGE_LAYOUT_RANGE_SIZE:
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

  vkCmdPipelineBarrier(
      impl,
      sourceStage, destStage,
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

bool VCommandBuffer::isSecondary() const {
  return fboLay.handler!=nullptr && fboLay.handler->impl!=VK_NULL_HANDLE;
  }

VkCommandBuffer VCommandBuffer::getBuffer(CmdBuff id) {
  if(bstate==nullptr)
    return impl;

  switch(id) {
    case CmdInline:
      if(bstate->state==NoPass)
        return impl;
      return bstate->adjustRp(impl,false);
    case CmdRenderpass:
      return bstate->adjustRp(impl,false);
    }
  assert(false && "invalid id value");
  }
