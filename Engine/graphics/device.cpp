#include "device.h"

#include <Tempest/Semaphore>
#include <Tempest/Fence>
#include <Tempest/CommandPool>
#include <Tempest/UniformsLayout>
#include <Tempest/UniformBuffer>
#include <Tempest/File>
#include <Tempest/Pixmap>

using namespace Tempest;

Device::Impl::Impl(AbstractGraphicsApi &api, SystemApi::Window *w, uint8_t maxFramesInFlight)
  :api(api),hwnd(w),maxFramesInFlight(maxFramesInFlight){
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

Device::Device(AbstractGraphicsApi &api, SystemApi::Window *w, uint8_t maxFramesInFlight)
  :api(api), impl(api,w,maxFramesInFlight), dev(impl.dev), swapchain(impl.swapchain),
   mainCmdPool(*this,api.createCommandPool(dev)),builtins(*this) {
  api.getCaps(dev,devCaps);
  }

Device::~Device() {
  }

void Device::waitIdle() {
  api.waitIdle(impl.dev);
  }

void Device::reset() {
  api.destroy(swapchain);
  swapchain=nullptr;
  impl.swapchain=nullptr;
  swapchain=api.createSwapchain(impl.hwnd,dev);
  impl.swapchain=swapchain;
  }

uint32_t Device::swapchainImageCount() const {
  return swapchain->imageCount();
  }

uint8_t Device::maxFramesInFlight() const {
  return impl.maxFramesInFlight;
  }

uint64_t Device::frameCounter() const {
  return framesCounter;
  }

uint32_t Device::imageId() const {
  return imgId;
  }

Frame Device::frame(uint32_t id) {
  Frame fr(*this,api.getImage(dev,swapchain,id),id);
  return fr;
  }

uint32_t Device::nextImage(Semaphore &onReady) {
  imgId = api.nextImage(dev,swapchain,onReady.impl.handler);
  return imgId;
  }

void Device::draw(const CommandBuffer &cmd, const Semaphore &wait) {
  api.draw(dev,swapchain,cmd.impl.handler,wait.impl.handler,nullptr,nullptr);
  }

void Device::draw(const CommandBuffer &cmd, const Semaphore &wait, Semaphore &done, Fence &fdone) {
  api.draw(dev,swapchain,cmd.impl.handler,wait.impl.handler,done.impl.handler,fdone.impl.handler);
  }

void Device::draw(const Tempest::CommandBuffer *cmd[], size_t count, const Semaphore &wait, Semaphore &done, Fence &fdone) {
  impl.cmdBuf.resize(count);
  for(size_t i=0;i<count;++i)
    impl.cmdBuf[i] = cmd[i]->impl.handler;
  api.draw(dev,swapchain,impl.cmdBuf.data(),count,wait.impl.handler,done.impl.handler,fdone.impl.handler);
  }

void Device::present(uint32_t img,const Semaphore &wait) {
  api.present(dev,swapchain,img,wait.impl.handler);
  framesCounter++;
  framesIdMod=(framesIdMod+1)%maxFramesInFlight();
  }

Shader Device::loadShader(const char *filename) {
  Tempest::RFile file(filename);

  const size_t fileSize=file.size();

  std::unique_ptr<uint32_t[]> buffer(new uint32_t[(fileSize+3)/4]);
  file.read(reinterpret_cast<char*>(buffer.get()),fileSize);

  size_t size=uint32_t(fileSize);

  Shader f(*this,api.createShader(dev,reinterpret_cast<const char*>(buffer.get()),size));
  return f;
  }

Shader Device::loadShader(const char *source, const size_t length) {
  Shader f(*this,api.createShader(dev,source,length));
  return f;
  }

const Device::Caps &Device::caps() const {
  return devCaps;
  }

Texture2d Device::createTexture(TextureFormat frm, const uint32_t w, const uint32_t h, const bool mips) {
  Texture2d t(*this,api.createTexture(dev,w,h,mips,frm),w,h,frm);
  return t;
  }

Texture2d Device::loadTexture(const Pixmap &pm, bool mips) {
  TextureFormat frm[]={
    TextureFormat::Alpha,
    TextureFormat::RGB8,
    TextureFormat::RGBA8,
    };
  Texture2d t(*this,api.createTexture(dev,pm,mips),pm.w(),pm.h(),frm[uint8_t(pm.format())]);
  return t;
  }

UniformBuffer Device::loadUbo(const void *mem, size_t size) {
  if(size==0)
    return UniformBuffer();
  VideoBuffer   data=createVideoBuffer(mem,size,MemUsage::UniformBit,BufferFlags::Dynamic);
  UniformBuffer ubo(std::move(data));
  return ubo;
  }

FrameBuffer Device::frameBuffer(Frame& out,RenderPass &pass) {
  FrameBuffer f(*this,api.createFbo(dev,swapchain,pass.impl.handler,out.id),swapchain->w(),swapchain->h());
  return f;
  }

FrameBuffer Device::frameBuffer(Frame &out, Texture2d &zbuf, RenderPass &pass) {
  FrameBuffer f(*this,api.createFbo(dev,swapchain,pass.impl.handler,out.id,zbuf.impl.handler),swapchain->w(),swapchain->h());
  return f;
  }

FrameBuffer Device::frameBuffer(Texture2d &out, Texture2d &zbuf, RenderPass &pass) {
  uint32_t w = uint32_t(out.w());
  uint32_t h = uint32_t(out.h());
  FrameBuffer f(*this,api.createFbo(dev,w,h,pass.impl.handler,out.impl.handler,zbuf.impl.handler),w,h);
  return f;
  }

RenderPass Device::pass(FboMode color, FboMode zbuf, TextureFormat zbufFormat) {
  RenderPass f(*this,api.createPass(dev,swapchain,zbufFormat,color,nullptr,zbuf,nullptr));
  return f;
  }

RenderPass Device::pass(const Color &color) {
  RenderPass f(*this,api.createPass(dev,swapchain,TextureFormat::Undefined,FboMode::PreserveOut,&color,FboMode::Clear,nullptr));
  return f;
  }

RenderPass Device::pass(const Color &color, FboMode zbuf, TextureFormat zbufFormat) {
  RenderPass f(*this,api.createPass(dev,swapchain,zbufFormat,FboMode::PreserveOut,&color,zbuf,nullptr));
  return f;
  }

RenderPass Device::pass(const Color &color, const float zbuf, TextureFormat zbufFormat) {
  RenderPass f(*this,api.createPass(dev,swapchain,zbufFormat,FboMode::PreserveOut,&color,FboMode::PreserveOut,&zbuf));
  return f;
  }

RenderPass Device::pass(const Color &color, const float zbuf, TextureFormat clFormat, TextureFormat zbufFormat) {
  RenderPass f(*this,api.createPass(dev,clFormat,zbufFormat,FboMode::PreserveOut,&color,FboMode::PreserveOut,&zbuf));
  return f;
  }

RenderPipeline Device::implPipeline(RenderPass& pass, uint32_t w, uint32_t h,
                                    const RenderState &st,const UniformsLayout &ulay,
                                    const Shader &vs, const Shader &fs,
                                    const Decl::ComponentType *decl, size_t declSize,
                                    size_t   stride,
                                    Topology tp) {
  if(w<=0 || h<=0 || !pass.impl || !vs.impl || !fs.impl)
    return RenderPipeline();

  RenderPipeline f(*this,api.createPipeline(dev,pass.impl.handler,w,h,st,decl,declSize,stride,tp,ulay,ulay.impl,{vs.impl.handler,fs.impl.handler}),w,h);
  return f;
  }

CommandBuffer Device::commandBuffer() {
  CommandBuffer buf(*this,api.createCommandBuffer(dev,mainCmdPool.impl.handler,CmdType::Primary));
  return buf;
  }

CommandBuffer Device::commandSecondaryBuffer() {
  CommandBuffer buf(*this,api.createCommandBuffer(dev,mainCmdPool.impl.handler,CmdType::Secondary));
  return buf;
  }

const Builtin &Device::builtin() const {
  return builtins;
  }

Fence Device::createFence() {
  Fence f(*this,api.createFence(dev));
  return f;
  }

Semaphore Device::createSemaphore() {
  Semaphore f(*this,api.createSemaphore(dev));
  return f;
  }

VideoBuffer Device::createVideoBuffer(const void *data, size_t size, MemUsage usage, BufferFlags flg) {
  VideoBuffer buf(*this,api.createBuffer(dev,data,size,usage,flg),size);
  return  buf;
  }

Uniforms Device::uniforms(const UniformsLayout &owner) {
  if(owner.impl==nullptr)
    owner.impl=api.createUboLayout(dev,owner);
  Uniforms ubo(*this,api.createDescriptors(dev,owner,owner.impl.get()));
  return ubo;
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

void Device::destroy(Uniforms &u) {
  api.destroy(u.desc.handler);
  }
