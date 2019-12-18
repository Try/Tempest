#include "headlessdevice.h"

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

HeadlessDevice::Impl::Impl(AbstractGraphicsApi &api, SystemApi::Window *w)
  :api(api) {
  dev=api.createDevice(w);
  }

HeadlessDevice::Impl::~Impl() {
  api.destroy(dev);
  }

HeadlessDevice::HeadlessDevice(AbstractGraphicsApi &api):HeadlessDevice(api,nullptr){
  }

HeadlessDevice::HeadlessDevice(AbstractGraphicsApi &api, SystemApi::Window *window)
  :api(api), impl(api,window), dev(impl.dev),
   mainCmdPool(*this,api.createCommandPool(dev)),builtins(*this) {
  api.getCaps(dev,devCaps);
  }

HeadlessDevice::~HeadlessDevice() {
  }

void HeadlessDevice::waitIdle() {
  api.waitIdle(impl.dev);
  }

void HeadlessDevice::draw(const PrimaryCommandBuffer &cmd, const Semaphore &wait) {
  api.draw(dev,cmd.impl.handler,wait.impl.handler,nullptr,nullptr);
  }

void HeadlessDevice::draw(const PrimaryCommandBuffer &cmd, Fence &fdone) {
  const Tempest::PrimaryCommandBuffer *c[] = {&cmd};
  draw(c,1,nullptr,0,nullptr,0,&fdone);
  }

void HeadlessDevice::draw(const PrimaryCommandBuffer &cmd, const Semaphore &wait, Semaphore &done, Fence &fdone) {
  api.draw(dev,cmd.impl.handler,wait.impl.handler,done.impl.handler,fdone.impl.handler);
  }

void HeadlessDevice::draw(const Tempest::PrimaryCommandBuffer *cmd[], size_t count,
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

void HeadlessDevice::implDraw(const Tempest::PrimaryCommandBuffer* cmd[],  AbstractGraphicsApi::CommandBuffer*  hcmd[],  size_t count,
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

Shader HeadlessDevice::loadShader(RFile &file) {
  const size_t fileSize=file.size();

  std::unique_ptr<uint32_t[]> buffer(new uint32_t[(fileSize+3)/4]);
  file.read(reinterpret_cast<char*>(buffer.get()),fileSize);

  size_t size=uint32_t(fileSize);

  Shader f(*this,api.createShader(dev,reinterpret_cast<const char*>(buffer.get()),size));
  return f;
  }

Shader HeadlessDevice::loadShader(const char *filename) {
  Tempest::RFile file(filename);
  return loadShader(file);
  }

Shader HeadlessDevice::loadShader(const char16_t *filename) {
  Tempest::RFile file(filename);
  return loadShader(file);
  }

Shader HeadlessDevice::loadShader(const char *source, const size_t length) {
  Shader f(*this,api.createShader(dev,source,length));
  return f;
  }

const HeadlessDevice::Caps &HeadlessDevice::caps() const {
  return devCaps;
  }

Texture2d HeadlessDevice::texture(TextureFormat frm, const uint32_t w, const uint32_t h, const bool mips) {
  if(!devCaps.hasSamplerFormat(frm) && !devCaps.hasAttachFormat(frm) && !devCaps.hasDepthFormat(frm))
    throw std::system_error(Tempest::GraphicsErrc::UnsupportedTextureFormat);
  uint32_t mipCnt = mips ? mipCount(w,h) : 1;
  Texture2d t(*this,api.createTexture(dev,w,h,mipCnt,frm),w,h,frm);
  return t;
  }

Texture2d HeadlessDevice::loadTexture(const Pixmap &pm, bool mips) {
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

Pixmap HeadlessDevice::readPixels(const Texture2d &t) {
  Pixmap pm;
  api.readPixels(dev,pm,t.impl,t.format(),uint32_t(t.w()),uint32_t(t.h()),0);
  return pm;
  }

UniformBuffer HeadlessDevice::loadUbo(const void *mem, size_t size) {
  if(size==0)
    return UniformBuffer();
  VideoBuffer   data=createVideoBuffer(mem,size,MemUsage::UniformBit,BufferFlags::Dynamic);
  UniformBuffer ubo(std::move(data));
  return ubo;
  }

FrameBuffer HeadlessDevice::frameBuffer(Texture2d &out, Texture2d &zbuf) {
  TextureFormat att[2] = {out.format(),zbuf.format()};
  uint32_t w = uint32_t(out.w());
  uint32_t h = uint32_t(out.h());

  if(out.w()!=zbuf.w() || out.h()!=zbuf.h())
    throw IncompleteFboException();

  FrameBufferLayout lay(api.createFboLayout(dev,w,h,nullptr,att,2),w,h);
  FrameBuffer f(*this,api.createFbo(dev,lay.impl.handler,w,h,out.impl.handler,zbuf.impl.handler),std::move(lay));
  return f;
  }

FrameBuffer HeadlessDevice::frameBuffer(Texture2d &out) {
  TextureFormat att[1] = {out.format()};
  uint32_t w = uint32_t(out.w());
  uint32_t h = uint32_t(out.h());

  FrameBufferLayout lay(api.createFboLayout(dev,w,h,nullptr,att,1),w,h);
  FrameBuffer f(*this,api.createFbo(dev,lay.impl.handler,w,h,out.impl.handler),std::move(lay));
  return f;
  }

RenderPass HeadlessDevice::pass(const Attachment &color) {
  const Attachment* att[1]={&color};
  RenderPass f(api.createPass(dev,att,1));
  return f;
  }

RenderPass HeadlessDevice::pass(const Attachment &color, const Attachment &depth) {
  const Attachment* att[2]={&color,&depth};
  RenderPass f(api.createPass(dev,att,2));
  return f;
  }

Fence HeadlessDevice::fence() {
  Fence f(*this,api.createFence(dev));
  return f;
  }

Semaphore HeadlessDevice::semaphore() {
  Semaphore f(*this,api.createSemaphore(dev));
  return f;
  }

RenderPipeline HeadlessDevice::implPipeline(const RenderState &st,const UniformsLayout &ulay,
                                            const Shader &vs, const Shader &fs,
                                            const Decl::ComponentType *decl, size_t declSize,
                                            size_t   stride,
                                            Topology tp) {
  if(!vs.impl || !fs.impl)
    return RenderPipeline();

  RenderPipeline f(*this,api.createPipeline(dev,st,decl,declSize,stride,tp,ulay,ulay.impl,{vs.impl.handler,fs.impl.handler}));
  return f;
  }

PrimaryCommandBuffer HeadlessDevice::commandBuffer() {
  PrimaryCommandBuffer buf(*this,api.createCommandBuffer(dev,mainCmdPool.impl.handler,nullptr,CmdType::Primary));
  return buf;
  }

CommandBuffer HeadlessDevice::commandSecondaryBuffer(const FrameBufferLayout &lay) {
  CommandBuffer buf(*this,api.createCommandBuffer(dev,mainCmdPool.impl.handler,
                                                  lay.impl.handler,CmdType::Secondary),
                    lay.w(),lay.h());
  return buf;
  }

CommandBuffer HeadlessDevice::commandSecondaryBuffer(const FrameBuffer &fbo) {
  return commandSecondaryBuffer(fbo.layout());
  }

const Builtin &HeadlessDevice::builtin() const {
  return builtins;
  }

const char *HeadlessDevice::renderer() const {
  return dev->renderer();
  }

AbstractGraphicsApi::Device *HeadlessDevice::implHandle() {
  return impl.dev;
  }

VideoBuffer HeadlessDevice::createVideoBuffer(const void *data, size_t size, MemUsage usage, BufferFlags flg) {
  VideoBuffer buf(*this,api.createBuffer(dev,data,size,usage,flg),size);
  return  buf;
  }

Uniforms HeadlessDevice::uniforms(const UniformsLayout &owner) {
  if(owner.impl==nullptr)
    owner.impl=api.createUboLayout(dev,owner);
  Uniforms ubo(*this,api.createDescriptors(dev,owner,owner.impl));
  return ubo;
  }

void HeadlessDevice::destroy(Uniforms &u) {
  api.destroy(u.desc.handler);
  }
