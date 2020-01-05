#include "device.h"

#include <Tempest/Semaphore>
#include <Tempest/Fence>
#include <Tempest/CommandPool>
#include <Tempest/UniformsLayout>
#include <Tempest/UniformBuffer>
#include <Tempest/File>
#include <Tempest/Pixmap>
#include <Tempest/Except>

using namespace Tempest;

static uint32_t mipCount(uint32_t w, uint32_t h) {
  uint32_t s = std::max(w,h);
  uint32_t n = 1;
  while(s>1) {
    ++n;
    s = s/2;
    }
  return n;
  }

Device::Impl::Impl(AbstractGraphicsApi &api, SystemApi::Window *w, uint32_t maxFramesInFlight)
  :api(api),maxFramesInFlight(maxFramesInFlight) {
  dev=api.createDevice(w);
  }

Device::Impl::~Impl() {
  api.destroy(dev);
  }

Device::Device(AbstractGraphicsApi &api, SystemApi::Window *window, uint32_t maxFramesInFlight)
  :api(api), impl(api,window,maxFramesInFlight), dev(impl.dev),
   mainCmdPool(*this,api.createCommandPool(dev)),builtins(*this) {
  api.getCaps(dev,devCaps);
  }

Device::~Device() {
  }

uint32_t Device::maxFramesInFlight() const {
  return impl.maxFramesInFlight;
  }

void Device::waitIdle() {
  impl.dev->waitIdle();
  }

void Device::draw(const PrimaryCommandBuffer &cmd, const Semaphore &wait) {
  api.draw(dev,cmd.impl.handler,wait.impl.handler,nullptr,nullptr);
  }

void Device::draw(const PrimaryCommandBuffer &cmd, Fence &fdone) {
  const Tempest::PrimaryCommandBuffer *c[] = {&cmd};
  draw(c,1,nullptr,0,nullptr,0,&fdone);
  }

void Device::draw(const PrimaryCommandBuffer &cmd, const Semaphore &wait, Semaphore &done, Fence &fdone) {
  api.draw(dev,cmd.impl.handler,wait.impl.handler,done.impl.handler,fdone.impl.handler);
  }

void Device::draw(const Tempest::PrimaryCommandBuffer *cmd[], size_t count,
                  const Semaphore *wait[], size_t waitCnt,
                  Semaphore *done[], size_t doneCnt,
                  Fence *fdone) {
  if(count+waitCnt+doneCnt<64){
    void* ptr[64];
    auto cx = reinterpret_cast<AbstractGraphicsApi::CommandBuffer**>(ptr);
    auto wx = reinterpret_cast<AbstractGraphicsApi::Semaphore**>(ptr+count);
    auto dx = reinterpret_cast<AbstractGraphicsApi::Semaphore**>(ptr+count+waitCnt);

    implDraw(cmd,  cx, count,
             wait, wx, waitCnt,
             done, dx, doneCnt,
             fdone->impl.handler);
    } else {
    std::unique_ptr<void*[]> ptr(new void*[count+waitCnt+doneCnt]);
    auto cx = reinterpret_cast<AbstractGraphicsApi::CommandBuffer**>(ptr.get());
    auto wx = reinterpret_cast<AbstractGraphicsApi::Semaphore**>(ptr.get()+count);
    auto dx = reinterpret_cast<AbstractGraphicsApi::Semaphore**>(ptr.get()+count+waitCnt);

    implDraw(cmd,  cx, count,
             wait, wx, waitCnt,
             done, dx, doneCnt,
             fdone->impl.handler);
    }
  }

void Device::implDraw(const Tempest::PrimaryCommandBuffer* cmd[],  AbstractGraphicsApi::CommandBuffer*  hcmd[],  size_t count,
                      const Semaphore*                     wait[], AbstractGraphicsApi::Semaphore*      hwait[], size_t waitCnt,
                      Semaphore*                           done[], AbstractGraphicsApi::Semaphore*      hdone[], size_t doneCnt,
                      AbstractGraphicsApi::Fence*          fdone) {
  for(size_t i=0;i<count;++i)
    hcmd[i] = cmd[i]->impl.handler;
  for(size_t i=0;i<waitCnt;++i)
    hwait[i] = wait[i]->impl.handler;
  for(size_t i=0;i<doneCnt;++i)
    hdone[i] = done[i]->impl.handler;

  api.draw(dev,
           hcmd, count,
           hwait, waitCnt,
           hdone, doneCnt,
           fdone);
  }

Shader Device::loadShader(RFile &file) {
  const size_t fileSize=file.size();

  std::unique_ptr<uint32_t[]> buffer(new uint32_t[(fileSize+3)/4]);
  file.read(reinterpret_cast<char*>(buffer.get()),fileSize);

  size_t size=uint32_t(fileSize);

  Shader f(*this,api.createShader(dev,reinterpret_cast<const char*>(buffer.get()),size));
  return f;
  }

Shader Device::loadShader(const char *filename) {
  Tempest::RFile file(filename);
  return loadShader(file);
  }

Shader Device::loadShader(const char16_t *filename) {
  Tempest::RFile file(filename);
  return loadShader(file);
  }

Shader Device::loadShader(const char *source, const size_t length) {
  Shader f(*this,api.createShader(dev,source,length));
  return f;
  }

Swapchain Device::swapchain(SystemApi::Window* w) const {
  return Swapchain(w,*impl.dev,api,2);
  }

const Device::Caps &Device::caps() const {
  return devCaps;
  }

Texture2d Device::texture(TextureFormat frm, const uint32_t w, const uint32_t h, const bool mips) {
  if(!devCaps.hasSamplerFormat(frm) && !devCaps.hasAttachFormat(frm) && !devCaps.hasDepthFormat(frm))
    throw std::system_error(Tempest::GraphicsErrc::UnsupportedTextureFormat);
  uint32_t mipCnt = mips ? mipCount(w,h) : 1;
  Texture2d t(*this,api.createTexture(dev,w,h,mipCnt,frm),w,h,frm);
  return t;
  }

Texture2d Device::loadTexture(const Pixmap &pm, bool mips) {
  static const TextureFormat frm[]={
    TextureFormat::Alpha,
    TextureFormat::RGB8,
    TextureFormat::RGBA8,
    TextureFormat::DXT1,
    TextureFormat::DXT3,
    TextureFormat::DXT5,
    };
  TextureFormat format = frm[uint8_t(pm.format())];
  uint32_t      mipCnt = mips ? mipCount(pm.w(),pm.h()) : 1;
  const Pixmap* p=&pm;
  Pixmap alt;

  if(isCompressedFormat(format)){
    if(devCaps.hasSamplerFormat(format) && (!mips || pm.mipCount()>1)){
      mipCnt = pm.mipCount();
      } else {
      alt    = Pixmap(pm,Pixmap::Format::RGBA);
      p      = &alt;
      format = TextureFormat::RGBA8;
      }
    }

  if(format==TextureFormat::RGB8 && !devCaps.hasSamplerFormat(format)){
    alt    = Pixmap(pm,Pixmap::Format::RGBA);
    p      = &alt;
    format = TextureFormat::RGBA8;
    }

  Texture2d t(*this,api.createTexture(dev,*p,format,mipCnt),p->w(),p->h(),format);
  return t;
  }

Pixmap Device::readPixels(const Texture2d &t) {
  Pixmap pm;
  api.readPixels(dev,pm,t.impl,t.format(),uint32_t(t.w()),uint32_t(t.h()),0);
  return pm;
  }

UniformBuffer Device::loadUbo(const void *mem, size_t size) {
  if(size==0)
    return UniformBuffer();
  VideoBuffer   data=createVideoBuffer(mem,size,MemUsage::UniformBit,BufferFlags::Dynamic);
  UniformBuffer ubo(std::move(data));
  return ubo;
  }

FrameBuffer Device::frameBuffer(Frame& out) {
  auto swapchain = out.swapchain;
  TextureFormat att[1] = {TextureFormat::Undefined};
  uint32_t w = swapchain->w();
  uint32_t h = swapchain->h();

  FrameBufferLayout lay(api.createFboLayout(dev,w,h,swapchain,att,1),w,h);
  FrameBuffer       f(*this,api.createFbo(dev,lay.impl.handler,swapchain,out.id),std::move(lay));
  return f;
  }

FrameBuffer Device::frameBuffer(Frame &out, Texture2d &zbuf) {
  auto swapchain = out.swapchain;

  TextureFormat att[2] = {TextureFormat::Undefined,zbuf.format()};
  uint32_t w = swapchain->w();
  uint32_t h = swapchain->h();

  if(int(w)!=zbuf.w() || int(h)!=zbuf.h())
    throw IncompleteFboException();

  FrameBufferLayout lay(api.createFboLayout(dev,w,h,swapchain,att,2),w,h);
  FrameBuffer       f(*this,api.createFbo(dev,lay.impl.handler,swapchain,out.id,zbuf.impl.handler),std::move(lay));
  return f;
  }

FrameBuffer Device::frameBuffer(Texture2d &out, Texture2d &zbuf) {
  TextureFormat att[2] = {out.format(),zbuf.format()};
  uint32_t w = uint32_t(out.w());
  uint32_t h = uint32_t(out.h());

  if(out.w()!=zbuf.w() || out.h()!=zbuf.h())
    throw IncompleteFboException();

  FrameBufferLayout lay(api.createFboLayout(dev,w,h,nullptr,att,2),w,h);
  FrameBuffer f(*this,api.createFbo(dev,lay.impl.handler,w,h,out.impl.handler,zbuf.impl.handler),std::move(lay));
  return f;
  }

FrameBuffer Device::frameBuffer(Texture2d &out) {
  TextureFormat att[1] = {out.format()};
  uint32_t w = uint32_t(out.w());
  uint32_t h = uint32_t(out.h());

  FrameBufferLayout lay(api.createFboLayout(dev,w,h,nullptr,att,1),w,h);
  FrameBuffer f(*this,api.createFbo(dev,lay.impl.handler,w,h,out.impl.handler),std::move(lay));
  return f;
  }

RenderPass Device::pass(const Attachment &color) {
  const Attachment* att[1]={&color};
  RenderPass f(api.createPass(dev,att,1));
  return f;
  }

RenderPass Device::pass(const Attachment &color, const Attachment &depth) {
  const Attachment* att[2]={&color,&depth};
  RenderPass f(api.createPass(dev,att,2));
  return f;
  }

Fence Device::fence() {
  Fence f(*this,api.createFence(dev));
  return f;
  }

Semaphore Device::semaphore() {
  Semaphore f(*this,api.createSemaphore(dev));
  return f;
  }

RenderPipeline Device::implPipeline(const RenderState &st,const UniformsLayout &ulay,
                                    const Shader &vs, const Shader &fs,
                                    const Decl::ComponentType *decl, size_t declSize,
                                    size_t   stride,
                                    Topology tp) {
  if(!vs.impl || !fs.impl)
    return RenderPipeline();

  RenderPipeline f(*this,api.createPipeline(dev,st,decl,declSize,stride,tp,ulay,ulay.impl,{vs.impl.handler,fs.impl.handler}));
  return f;
  }

PrimaryCommandBuffer Device::commandBuffer() {
  PrimaryCommandBuffer buf(*this,api.createCommandBuffer(dev,mainCmdPool.impl.handler,nullptr,CmdType::Primary));
  return buf;
  }

CommandBuffer Device::commandSecondaryBuffer(const FrameBufferLayout &lay) {
  CommandBuffer buf(*this,api.createCommandBuffer(dev,mainCmdPool.impl.handler,
                                                  lay.impl.handler,CmdType::Secondary),
                    lay.mw,lay.mh);
  return buf;
  }

CommandBuffer Device::commandSecondaryBuffer(const FrameBuffer &fbo) {
  return commandSecondaryBuffer(fbo.layout());
  }

const Builtin &Device::builtin() const {
  return builtins;
  }

const char* Device::renderer() const {
  return dev->renderer();
  }

VideoBuffer Device::createVideoBuffer(const void *data, size_t size, MemUsage usage, BufferFlags flg) {
  VideoBuffer buf(*this,api.createBuffer(dev,data,size,usage,flg),size);
  return  buf;
  }

Uniforms Device::uniforms(const UniformsLayout &owner) {
  if(owner.impl==nullptr)
    owner.impl=api.createUboLayout(dev,owner);
  Uniforms ubo(*this,api.createDescriptors(dev,owner,owner.impl));
  return ubo;
  }

