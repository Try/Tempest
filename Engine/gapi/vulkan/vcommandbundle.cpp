#include "vcommandbundle.h"

#include "vbuffer.h"
#include "vcommandpool.h"
#include "vdescriptorarray.h"
#include "vdevice.h"
#include "vframebufferlayout.h"
#include "vpipeline.h"

using namespace Tempest;
using namespace Tempest::Detail;

VCommandBundle::VCommandBundle(VkDevice& device, VCommandPool& pool, VFramebufferLayout* fbo)
  :device(device), pool(&pool), fboLay(fbo) {
  VkCommandBufferAllocateInfo allocInfo = {};
  allocInfo.sType             =VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocInfo.commandPool       =pool.impl;
  allocInfo.level             =VK_COMMAND_BUFFER_LEVEL_SECONDARY;
  allocInfo.commandBufferCount=1;

  vkAssert(vkAllocateCommandBuffers(device,&allocInfo,&impl));
  }

VCommandBundle::VCommandBundle(VDevice& device, VFramebufferLayout* fbo)
  :device(device.device), fboLay(fbo) {
  ownPool = true;
  pool    = new VCommandPool(device);
  try {
    VkCommandBufferAllocateInfo allocInfo = {};
    allocInfo.sType             =VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool       =pool->impl;
    allocInfo.level             =VK_COMMAND_BUFFER_LEVEL_SECONDARY;
    allocInfo.commandBufferCount=1;

    vkAssert(vkAllocateCommandBuffers(device.device,&allocInfo,&impl));
    }
  catch (...) {
    delete pool;
    throw;
    }
  }

VCommandBundle::VCommandBundle(VCommandBundle&& other)
  :device(other.device),pool(other.pool), impl(other.impl), fboLay(other.fboLay) {
  other.device = nullptr;
  other.impl   = nullptr;
  }

VCommandBundle::~VCommandBundle() {
  if(device==nullptr || impl==nullptr)
    return;
  vkFreeCommandBuffers(device,pool->impl,1,&impl);
  if(ownPool)
    delete pool;
  }

VCommandBundle& VCommandBundle::operator =(VCommandBundle&& other) {
  std::swap(device,other.device);
  std::swap(pool,other.pool);
  std::swap(impl,other.impl);
  std::swap(recording,other.recording);
  std::swap(fboLay,other.fboLay);

  return *this;
  }

void VCommandBundle::reset() {
  vkResetCommandBuffer(impl,0);//VK_COMMAND_BUFFER_RESET_RELEASE_RESOURCES_BIT);
  }

void VCommandBundle::begin() {
  VkCommandBufferUsageFlags      usageFlags = VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;
  VkCommandBufferInheritanceInfo inheritanceInfo={};

  inheritanceInfo.sType       = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
  inheritanceInfo.framebuffer = VK_NULL_HANDLE;
  inheritanceInfo.renderPass  = fboLay.handler->impl;

  VkCommandBufferBeginInfo beginInfo = {};
  beginInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags            = usageFlags;
  beginInfo.pInheritanceInfo = &inheritanceInfo;

  vkAssert(vkBeginCommandBuffer(impl,&beginInfo));
  recording = true;
  }

void VCommandBundle::end() {
  recording = false;
  vkAssert(vkEndCommandBuffer(impl));
  }

bool VCommandBundle::isRecording() const {
  return recording;
  }

void VCommandBundle::setPipeline(AbstractGraphicsApi::Pipeline& p, uint32_t w, uint32_t h) {
  VPipeline&           px = reinterpret_cast<VPipeline&>(p);
  VFramebufferLayout*  l  = reinterpret_cast<VFramebufferLayout*>(fboLay.handler);
  auto& v = px.instance(*l,w,h);
  vkCmdBindPipeline(impl,VK_PIPELINE_BIND_POINT_GRAPHICS,v.val);
  }

void VCommandBundle::setBytes(AbstractGraphicsApi::Pipeline& p, void* data, size_t size) {
  VPipeline&        px=reinterpret_cast<VPipeline&>(p);
  vkCmdPushConstants(impl, px.pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, uint32_t(size), data);
  }

void VCommandBundle::setUniforms(AbstractGraphicsApi::Pipeline& p, AbstractGraphicsApi::Desc& u) {
  VPipeline&        px=reinterpret_cast<VPipeline&>(p);
  VDescriptorArray& ux=reinterpret_cast<VDescriptorArray&>(u);
  vkCmdBindDescriptorSets(impl,VK_PIPELINE_BIND_POINT_GRAPHICS,
                          px.pipelineLayout,0,
                          1,&ux.desc,
                          0,nullptr);
  }

void VCommandBundle::setViewport(const Rect& r) {
  VkViewport vk={};
  vk.x        = float(r.x);
  vk.y        = float(r.y);
  vk.width    = float(r.w);
  vk.height   = float(r.h);
  vk.minDepth = 0;
  vk.maxDepth = 1;
  vkCmdSetViewport(impl,0,1,&vk);
  }

void VCommandBundle::setVbo(const AbstractGraphicsApi::Buffer& b) {
  const VBuffer& vbo=reinterpret_cast<const VBuffer&>(b);

  std::initializer_list<VkBuffer>     buffers = {vbo.impl};
  std::initializer_list<VkDeviceSize> offsets = {0};
  vkCmdBindVertexBuffers(
        impl, 0, uint32_t(buffers.size()),
        buffers.begin(),
        offsets.begin() );
  }

void VCommandBundle::setIbo(const AbstractGraphicsApi::Buffer& b, IndexClass cls) {
  static const VkIndexType type[]={
    VK_INDEX_TYPE_UINT16,
    VK_INDEX_TYPE_UINT32
    };

  const VBuffer& ibo=reinterpret_cast<const VBuffer&>(b);
  vkCmdBindIndexBuffer(impl,ibo.impl,0,type[uint32_t(cls)]);
  }

void VCommandBundle::draw(size_t offset, size_t size) {
  vkCmdDraw(impl,uint32_t(size), 1, uint32_t(offset),0);
  }

void VCommandBundle::drawIndexed(size_t ioffset, size_t isize, size_t voffset) {
  vkCmdDrawIndexed(impl,uint32_t(isize),1, uint32_t(ioffset), int32_t(voffset),0);
  }
