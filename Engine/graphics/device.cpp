#include "device.h"

#include <Tempest/Semaphore>
#include <Tempest/Fence>
#include <Tempest/UniformsLayout>
#include <Tempest/UniformBuffer>
#include <Tempest/File>
#include <Tempest/Pixmap>
#include <Tempest/Except>

#include <mutex>

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

static TextureFormat toTextureFormat(Pixmap::Format f) {
  switch(f) {
    case Pixmap::Format::R:    return TextureFormat::R8;
    case Pixmap::Format::RG:   return TextureFormat::RG8;
    case Pixmap::Format::RGB:  return TextureFormat::RGB8;
    case Pixmap::Format::RGBA: return TextureFormat::RGBA8;

    case Pixmap::Format::DXT1: return TextureFormat::DXT1;
    case Pixmap::Format::DXT3: return TextureFormat::DXT3;
    case Pixmap::Format::DXT5: return TextureFormat::DXT5;
    }
  }

Device::Impl::Impl(AbstractGraphicsApi &api, const char* name, uint8_t maxFramesInFlight)
  :api(api),maxFramesInFlight(maxFramesInFlight) {
  dev=api.createDevice(name);
  }

Device::Impl::~Impl() {
  api.destroy(dev);
  }

Device::Device(AbstractGraphicsApi& api, uint8_t maxFramesInFlight)
  :Device(api,nullptr,maxFramesInFlight){
  }

Device::Device(AbstractGraphicsApi &api, const char* name, uint8_t maxFramesInFlight)
  :api(api), impl(api,name,maxFramesInFlight), dev(impl.dev), builtins(*this) {
  api.getCaps(dev,devProps);
  }

Device::~Device() {
  }

uint8_t Device::maxFramesInFlight() const {
  return impl.maxFramesInFlight;
  }

void Device::waitIdle() {
  impl.dev->waitIdle();
  }

void Device::submit(const PrimaryCommandBuffer &cmd, const Semaphore &wait) {
  api.submit(dev,cmd.impl.handler,wait.impl.handler,nullptr,nullptr);
  }

void Device::submit(const PrimaryCommandBuffer &cmd, Fence &fdone) {
  const Tempest::PrimaryCommandBuffer *c[] = {&cmd};
  submit(c,1,nullptr,0,nullptr,0,&fdone);
  }

void Device::submit(const PrimaryCommandBuffer &cmd, const Semaphore &wait, Semaphore &done, Fence &fdone) {
  api.submit(dev,cmd.impl.handler,wait.impl.handler,done.impl.handler,fdone.impl.handler);
  }

void Device::submit(const Tempest::PrimaryCommandBuffer *cmd[], size_t count,
                    const Semaphore *wait[], size_t waitCnt,
                    Semaphore *done[], size_t doneCnt,
                    Fence *fdone) {
  if(count+waitCnt+doneCnt<64){
    void* ptr[64];
    auto cx = reinterpret_cast<AbstractGraphicsApi::CommandBuffer**>(ptr);
    auto wx = reinterpret_cast<AbstractGraphicsApi::Semaphore**>(ptr+count);
    auto dx = reinterpret_cast<AbstractGraphicsApi::Semaphore**>(ptr+count+waitCnt);

    implSubmit(cmd,  cx, count,
               wait, wx, waitCnt,
               done, dx, doneCnt,
               fdone->impl.handler);
    } else {
    std::unique_ptr<void*[]> ptr(new void*[count+waitCnt+doneCnt]);
    auto cx = reinterpret_cast<AbstractGraphicsApi::CommandBuffer**>(ptr.get());
    auto wx = reinterpret_cast<AbstractGraphicsApi::Semaphore**>(ptr.get()+count);
    auto dx = reinterpret_cast<AbstractGraphicsApi::Semaphore**>(ptr.get()+count+waitCnt);

    implSubmit(cmd,  cx, count,
               wait, wx, waitCnt,
               done, dx, doneCnt,
               fdone->impl.handler);
    }
  }

void Device::present(Swapchain& sw, uint32_t img, const Semaphore& wait) {
  api.present(dev,sw.impl.handler,img,wait.impl.handler);
  sw.framesCounter++;
  sw.framesIdMod=(sw.framesIdMod+1)%maxFramesInFlight();
  }

void Device::implSubmit(const Tempest::PrimaryCommandBuffer* cmd[],  AbstractGraphicsApi::CommandBuffer*  hcmd[],  size_t count,
                        const Semaphore*                     wait[], AbstractGraphicsApi::Semaphore*      hwait[], size_t waitCnt,
                        Semaphore*                           done[], AbstractGraphicsApi::Semaphore*      hdone[], size_t doneCnt,
                        AbstractGraphicsApi::Fence*          fdone) {
  for(size_t i=0;i<count;++i)
    hcmd[i] = cmd[i]->impl.handler;
  for(size_t i=0;i<waitCnt;++i)
    hwait[i] = wait[i]->impl.handler;
  for(size_t i=0;i<doneCnt;++i)
    hdone[i] = done[i]->impl.handler;

  api.submit(dev,
             hcmd, count,
             hwait, waitCnt,
             hdone, doneCnt,
             fdone);
  }

void Device::createInstance(const UniformsLayout& ulay) {
  if(ulay.impl.handler==nullptr) {
    std::lock_guard<Detail::SpinLock> guard(ulay.sync);
    if(ulay.impl.handler==nullptr)
      ulay.impl=api.createUboLayout(dev,ulay);
    }
  }

Shader Device::loadShader(RFile &file) {
  const size_t fileSize=file.size();

  std::unique_ptr<uint8_t[]> buffer(new uint8_t[fileSize]);
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

Shader Device::shader(const void *source, const size_t length) {
  Shader f(*this,api.createShader(dev,source,length));
  return f;
  }

Swapchain Device::swapchain(SystemApi::Window* w) const {
  return Swapchain(api.createSwapchain(w,impl.dev));
  }

const Device::Props& Device::properties() const {
  return devProps;
  }

Attachment Device::attachment(TextureFormat frm, const uint32_t w, const uint32_t h, const bool mips) {
  if(!devProps.hasSamplerFormat(frm) && !devProps.hasAttachFormat(frm) && !devProps.hasDepthFormat(frm))
    throw std::system_error(Tempest::GraphicsErrc::UnsupportedTextureFormat);
  uint32_t mipCnt = mips ? mipCount(w,h) : 1;
  Texture2d t(*this,api.createTexture(dev,w,h,mipCnt,frm),w,h,frm);
  return Attachment(std::move(t));
  }

Texture2d Device::loadTexture(const Pixmap &pm, bool mips) {
  TextureFormat format = toTextureFormat(pm.format());
  uint32_t      mipCnt = mips ? mipCount(pm.w(),pm.h()) : 1;
  const Pixmap* p=&pm;
  Pixmap alt;

  if(isCompressedFormat(format)){
    if(devProps.hasSamplerFormat(format) && (!mips || pm.mipCount()>1)){
      mipCnt = pm.mipCount();
      } else {
      alt    = Pixmap(pm,Pixmap::Format::RGBA);
      p      = &alt;
      format = TextureFormat::RGBA8;
      }
    }

  if(format==TextureFormat::RGB8 && !devProps.hasSamplerFormat(format)){
    alt    = Pixmap(pm,Pixmap::Format::RGBA);
    p      = &alt;
    format = TextureFormat::RGBA8;
    }

  Texture2d t(*this,api.createTexture(dev,*p,format,mipCnt),p->w(),p->h(),format);
  return t;
  }

Pixmap Device::readPixels(const Texture2d &t) {
  Pixmap pm;
  api.readPixels(dev,pm,t.impl,TextureLayout::Sampler,t.format(),uint32_t(t.w()),uint32_t(t.h()),0);
  return pm;
  }

Pixmap Device::readPixels(const Attachment& t) {
  Pixmap pm;
  auto& tx = textureCast(t);
  api.readPixels(dev,pm,tx.impl,TextureLayout::Sampler,tx.format(),uint32_t(t.w()),uint32_t(t.h()),0);
  return pm;
  }

TextureFormat Device::formatOf(const Attachment& a) {
  if(a.sImpl.swapchain!=nullptr)
    return TextureFormat::Undefined;
  return a.tImpl.frm;
  }

FrameBuffer Device::frameBuffer(Attachment& out, Attachment& zbuf) {
  TextureFormat att[2] = {formatOf(out),formatOf(zbuf)};
  uint32_t w = uint32_t(out.w());
  uint32_t h = uint32_t(out.h());

  if(out.w()!=zbuf.w() || out.h()!=zbuf.h())
    throw IncompleteFboException();

  auto zImpl = textureCast(zbuf).impl.handler;
  if(out.sImpl.swapchain!=nullptr) {
    auto swapchain = out.sImpl.swapchain;
    auto sId       = out.sImpl.id;

    FrameBufferLayout lay(api.createFboLayout(dev,swapchain,att,2));
    FrameBuffer       f(*this,api.createFbo(dev,lay.impl.handler,swapchain,sId,zImpl),std::move(lay),w,h);
    return f;
    } else {
    FrameBufferLayout lay(api.createFboLayout(dev,nullptr,att,2));
    FrameBuffer f(*this,api.createFbo(dev,lay.impl.handler,w,h,out.tImpl.impl.handler,zImpl),std::move(lay),w,h);
    return f;
    }
  }

FrameBuffer Device::frameBuffer(Attachment &out) {
  TextureFormat att[1] = {formatOf(out)};
  uint32_t w = uint32_t(out.w());
  uint32_t h = uint32_t(out.h());

  if(out.sImpl.swapchain!=nullptr) {
    auto swapchain = out.sImpl.swapchain;
    auto sId       = out.sImpl.id;

    FrameBufferLayout lay(api.createFboLayout(dev,swapchain,att,1));
    FrameBuffer       f(*this,api.createFbo(dev,lay.impl.handler,swapchain,sId),std::move(lay),w,h);
    return f;
    } else {
    FrameBufferLayout lay(api.createFboLayout(dev,nullptr,att,1));
    FrameBuffer f(*this,api.createFbo(dev,lay.impl.handler,w,h,out.tImpl.impl.handler),std::move(lay),w,h);
    return f;
    }
  }

RenderPass Device::pass(const FboMode &color) {
  const FboMode* att[1]={&color};
  RenderPass f(api.createPass(dev,att,1));
  return f;
  }

RenderPass Device::pass(const FboMode& color, const FboMode& depth) {
  const FboMode* att[2]={&color,&depth};
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

  createInstance(ulay);
  RenderPipeline f(*this,api.createPipeline(dev,st,decl,declSize,stride,tp,*ulay.impl.handler,{vs.impl.handler,fs.impl.handler}));
  return f;
  }

PrimaryCommandBuffer Device::commandBuffer() {
  PrimaryCommandBuffer buf(*this,api.createCommandBuffer(dev));
  return buf;
  }

CommandBuffer Device::commandSecondaryBuffer(const FrameBufferLayout &lay) {
  CommandBuffer buf(*this,api.createCommandBuffer(dev,lay.impl.handler));
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

VideoBuffer Device::createVideoBuffer(const void *data, size_t count, size_t size, size_t alignedSz, MemUsage usage, BufferFlags flg) {
  VideoBuffer buf(*this,api.createBuffer(dev,data,count,size,alignedSz,usage,flg),count*alignedSz);
  return  buf;
  }

Uniforms Device::uniforms(const UniformsLayout &ulay) {
  createInstance(ulay);
  Uniforms ubo(*this,api.createDescriptors(dev,ulay,*ulay.impl.handler));
  return ubo;
  }

