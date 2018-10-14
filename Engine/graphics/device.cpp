#include "device.h"

#include <Tempest/Semaphore>
#include <Tempest/Fence>
#include <Tempest/CommandPool>

using namespace Tempest;

Device::Impl::Impl(AbstractGraphicsApi &api, SystemApi::Window *w)
  :api(api){
  dev=api.createDevice(w);
  try {
    swapchain=api.createSwapchain(w,dev);
    }
  catch(...){
    api.destroy(dev);
    throw;
  }
  }

Device::Impl::~Impl() {
  api.destroy(swapchain);
  api.destroy(dev);
  }

Device::Device(AbstractGraphicsApi &api, SystemApi::Window *w)
  :api(api), impl(api,w), dev(impl.dev), swapchain(impl.swapchain),
   mainCmdPool(*this,api.createCommandPool(dev)) {
  }

Device::~Device() {
  }

Frame Device::frame(uint32_t id) {
  Frame fr(*this,api.getImage(dev,swapchain,id),id);
  return fr;
  }

uint32_t Device::nextImage(Semaphore &onReady) {
  return api.nextImage(dev,swapchain,onReady.impl.handler);
  }

void Device::draw(const CommandBuffer &cmd, const Semaphore &wait, Semaphore &done, Fence &fdone) {
  api.draw(dev,swapchain,cmd.impl.handler,wait.impl.handler,done.impl.handler,fdone.impl.handler);
  }

void Device::present(uint32_t img,const Semaphore &wait) {
  api.present(dev,swapchain,img,wait.impl.handler);
  }

Shader Device::loadShader(const char *filename) {
  Shader f(*this,api.createShader(dev,filename));
  return f;
  }

FrameBuffer Device::frameBuffer(Frame& out,RenderPass &pass) {
  FrameBuffer f(*this,api.createFbo(dev,swapchain,pass.impl.handler,out.id));
  return f;
  }

RenderPass Device::pass() {
  RenderPass f(*this,api.createPass(dev,swapchain));
  return f;
  }

RenderPipeline Device::pipeline(RenderPass& pass,uint32_t w,uint32_t h,Shader& vs,Shader& fs) {
  if(w<=0 || h<=0 || !pass.impl || !vs.impl || !fs.impl)
    return RenderPipeline();

  RenderPipeline f(*this,api.createPipeline(dev,pass.impl.handler,w,h,{vs.impl.handler,fs.impl.handler}));
  return f;
  }

CommandBuffer Device::commandBuffer() {
  CommandBuffer buf(*this,api.createCommandBuffer(dev,mainCmdPool.impl.handler));
  return buf;
  }

Fence Device::createFence() {
  Fence f(*this,api.createFence(dev));
  return f;
  }

Semaphore Device::createSemaphore() {
  Semaphore f(*this,api.createSemaphore(dev));
  return f;
  }

VideoBuffer Device::createVideoBuffer(const void *data,size_t size,AbstractGraphicsApi::MemUsage usage) {
  VideoBuffer buf(*this,api.createBuffer(dev,data,size,usage));
  return  buf;
  }

Uniforms Device::loadUniforms(const void *mem, size_t size) {
  VideoBuffer data=createVideoBuffer(mem,size,AbstractGraphicsApi::MemUsage::UniformBit);
  Uniforms    ubo(std::move(data));
  return ubo;
  }

void Device::destroy(RenderPipeline &p) {
  api.destroy(p.impl.handler);
  }

void Device::destroy(RenderPass &p) {
  api.destroy(p.impl.handler);
  }

void Device::destroy(FrameBuffer &f) {
  api.destroy(f.impl.handler);
  }

void Device::destroy(Shader &p) {
  api.destroy(p.impl.handler);
  }

void Device::destroy(Fence &f) {
  api.destroy(f.impl.handler);
  }

void Device::destroy(Semaphore &s) {
  api.destroy(s.impl.handler);
  }

void Device::destroy(CommandPool &p) {
  api.destroy(p.impl.handler);
  }

void Device::destroy(CommandBuffer &c) {
  api.destroy(c.impl.handler);
  }

void Device::destroy(VideoBuffer &b) {
  api.destroy(b.impl.handler);
  }
