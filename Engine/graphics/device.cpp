#include "device.h"

#include <Tempest/Fence>
#include <Tempest/PipelineLayout>
#include <Tempest/UniformBuffer>
#include <Tempest/File>
#include <Tempest/Pixmap>
#include <Tempest/Except>

#include <mutex>
#include <cassert>

#include "utility/smallarray.h"

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

Device::Impl::Impl(AbstractGraphicsApi &api, const char* name)
  :api(api) {
  dev=api.createDevice(name);
  }

Device::Impl::Impl(AbstractGraphicsApi& api, DeviceType type)
  :api(api) {
  if(type==DeviceType::Unknown) {
    dev=api.createDevice(nullptr);
    return;
    }

  auto d = api.devices();
  for(auto& i:d)
    if(i.type==type) {
      dev=api.createDevice(i.name);
      return;
      }
  throw std::system_error(Tempest::GraphicsErrc::NoDevice);
  }

Device::Impl::~Impl() {
  api.destroy(dev);
  }

Device::Device(AbstractGraphicsApi& api)
  :Device(api,nullptr){
  }

Device::Device(AbstractGraphicsApi &api, const char* name)
  :api(api), impl(api,name), dev(impl.dev), builtins(*this) {
  api.getCaps(dev,devProps);
  }

Device::Device(AbstractGraphicsApi& api, DeviceType type)
  :api(api), impl(api,type), dev(impl.dev), builtins(*this) {
  api.getCaps(dev,devProps);
  }

Device::~Device() {
  }

void Device::waitIdle() {
  impl.dev->waitIdle();
  }

void Device::submit(const CommandBuffer &cmd) {
  api.submit(dev,cmd.impl.handler,nullptr);
  }

void Device::submit(const CommandBuffer &cmd, Fence &fdone) {
  api.submit(dev,cmd.impl.handler,fdone.impl.handler);
  }

void Device::present(Swapchain& sw) {
  api.present(dev,sw.impl.handler);
  }

Shader Device::shader(RFile &file) {
  const size_t fileSize=file.size();

  std::unique_ptr<uint8_t[]> buffer(new uint8_t[fileSize]);
  file.read(reinterpret_cast<char*>(buffer.get()),fileSize);

  size_t size=uint32_t(fileSize);

  Shader f(*this,api.createShader(dev,reinterpret_cast<const char*>(buffer.get()),size));
  return f;
  }

Shader Device::shader(const char *filename) {
  Tempest::RFile file(filename);
  return shader(file);
  }

Shader Device::shader(const char16_t *filename) {
  Tempest::RFile file(filename);
  return shader(file);
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
  if(!devProps.hasSamplerFormat(frm) && !devProps.hasAttachFormat(frm))
    throw std::system_error(Tempest::GraphicsErrc::UnsupportedTextureFormat);
  if(w>devProps.tex2d.maxSize || h>devProps.tex2d.maxSize)
    throw std::system_error(Tempest::GraphicsErrc::UnsupportedTextureFormat);
  uint32_t mipCnt = mips ? mipCount(w,h) : 1;
  Texture2d t(*this,api.createTexture(dev,w,h,mipCnt,frm),w,h,frm);
  return Attachment(std::move(t));
  }

ZBuffer Device::zbuffer(TextureFormat frm, const uint32_t w, const uint32_t h) {
  if(!devProps.hasDepthFormat(frm))
    throw std::system_error(Tempest::GraphicsErrc::UnsupportedTextureFormat);
  if(w>devProps.tex2d.maxSize || h>devProps.tex2d.maxSize)
    throw std::system_error(Tempest::GraphicsErrc::UnsupportedTextureFormat);
  Texture2d t(*this,api.createTexture(dev,w,h,1,frm),w,h,frm);
  return ZBuffer(std::move(t),devProps.hasSamplerFormat(frm));
  }

Texture2d Device::texture(const Pixmap &pm, const bool mips) {
  TextureFormat format = Pixmap::toTextureFormat(pm.format());
  uint32_t      mipCnt = mips ? mipCount(pm.w(),pm.h()) : 1;
  const Pixmap* p=&pm;
  Pixmap        alt;

  if(pm.w()>devProps.tex2d.maxSize || pm.h()>devProps.tex2d.maxSize)
    throw std::system_error(Tempest::GraphicsErrc::UnsupportedTextureFormat);

  if(isCompressedFormat(format)){
    if(devProps.hasSamplerFormat(format) && (!mips || pm.mipCount()>1)){
      mipCnt = pm.mipCount();
      } else {
      alt    = Pixmap(pm,Pixmap::Format::RGBA);
      p      = &alt;
      format = TextureFormat::RGBA8;
      }
    }

  if(!devProps.hasSamplerFormat(format)) {
    if(format==TextureFormat::RGB8){
      alt    = Pixmap(pm,Pixmap::Format::RGBA);
      p      = &alt;
      format = TextureFormat::RGBA8;
      }
    else if(format==TextureFormat::RGB16){
      alt    = Pixmap(pm,Pixmap::Format::RGBA16);
      p      = &alt;
      format = TextureFormat::RGBA16;
      }
    else if(format==TextureFormat::RGB32F){
      alt    = Pixmap(pm,Pixmap::Format::RGBA32F);
      p      = &alt;
      format = TextureFormat::RGBA32F;
      }
    else {
      throw std::system_error(Tempest::GraphicsErrc::UnsupportedTextureFormat);
      }
    }

  Texture2d t(*this,api.createTexture(dev,*p,format,mipCnt),p->w(),p->h(),format);
  return t;
  }

StorageImage Device::image2d(TextureFormat frm, const uint32_t w, const uint32_t h, const bool mips) {
  if(!devProps.hasStorageFormat(frm))
    throw std::system_error(Tempest::GraphicsErrc::UnsupportedTextureFormat);
  if(w>devProps.tex2d.maxSize || h>devProps.tex2d.maxSize)
    throw std::system_error(Tempest::GraphicsErrc::UnsupportedTextureFormat);
  uint32_t mipCnt = mips ? mipCount(w,h) : 1;
  Texture2d t(*this,api.createStorage(dev,w,h,mipCnt,frm),w,h,frm);
  return StorageImage(std::move(t));
  }

StorageImage Device::image3d(TextureFormat frm, const uint32_t w, const uint32_t h, const uint32_t d, const bool mips) {
  if(!devProps.hasStorageFormat(frm))
    throw std::system_error(Tempest::GraphicsErrc::UnsupportedTextureFormat);
  if(w>devProps.tex2d.maxSize || h>devProps.tex2d.maxSize)
    throw std::system_error(Tempest::GraphicsErrc::UnsupportedTextureFormat);
  uint32_t mipCnt = mips ? mipCount(w,h) : 1;
  Texture2d t(*this,api.createStorage(dev,w,h,d,mipCnt,frm),w,h,frm);
  return StorageImage(std::move(t));
  }

AccelerationStructure Device::implBlas(const Detail::VideoBuffer& vbo, size_t stride, const Detail::VideoBuffer& ibo, Detail::IndexClass icls, size_t offset, size_t count) {
  if(!properties().raytracing.rayQuery)
    throw std::system_error(Tempest::GraphicsErrc::UnsupportedExtension);
  assert(3*sizeof(float)<=stride); // float3 positions, no overlap
  auto blas = api.createBottomAccelerationStruct(dev,
                                                 vbo.impl.handler,vbo.size()/stride,stride,
                                                 ibo.impl.handler,count,offset,icls);
  return AccelerationStructure(*this,blas);
  }

AccelerationStructure Device::tlas(std::initializer_list<RtInstance> geom) {
  return tlas(geom.begin(),geom.size());
  }

AccelerationStructure Device::tlas(const std::vector<RtInstance>& geom) {
  return tlas(geom.data(),geom.size());
  }

AccelerationStructure Device::tlas(const RtInstance* geom, size_t geomSize) {
  std::vector<AbstractGraphicsApi::AccelerationStructure*> as(geomSize);
  for(size_t i=0; i<geomSize; ++i)
    as[i] = geom[i].blas->impl.handler;
  auto tlas = api.createTopAccelerationStruct(dev,geom,as.data(),geomSize);
  return AccelerationStructure(*this,tlas);
  }

Pixmap Device::readPixels(const Texture2d &t, uint32_t mip) {
  Pixmap pm;
  api.readPixels(dev,pm,t.impl,t.format(),uint32_t(t.w()),uint32_t(t.h()),mip,false);
  return pm;
  }

Pixmap Device::readPixels(const Attachment& t, uint32_t mip) {
  Pixmap pm;
  auto& tx = textureCast(t);
  uint32_t w = t.w();
  uint32_t h = t.h();
  for(uint32_t i=0; i<mip; ++i) {
    w = (w==1 ? 1 : w/2);
    h = (h==1 ? 1 : h/2);
    }
  api.readPixels(dev,pm,tx.impl,tx.format(),w,h,mip,false);
  return pm;
  }

Pixmap Device::readPixels(const StorageImage& t, uint32_t mip) {
  Pixmap pm;
  uint32_t w = t.w();
  uint32_t h = t.h();
  for(uint32_t i=0; i<mip; ++i) {
    w = (w==1 ? 1 : w/2);
    h = (h==1 ? 1 : h/2);
    }
  api.readPixels(dev,pm,t.tImpl.impl,t.format(),w,h,mip,true);
  return pm;
  }

void Device::readBytes(const StorageBuffer& ssbo, void* out, size_t size) {
  api.readBytes(dev,ssbo.impl.impl.handler,out,size);
  }

Fence Device::fence() {
  Fence f(*this,api.createFence(dev));
  return f;
  }

ComputePipeline Device::pipeline(const Shader& comp) {
  if(!comp.impl)
    return ComputePipeline();

  auto ulay = api.createPipelineLayout(dev,&comp.impl.handler,1);
  auto pipe = api.createComputePipeline(dev,*ulay.handler,comp.impl.handler);
  ComputePipeline f(std::move(pipe),std::move(ulay));
  return f;
  }

RenderPipeline Device::pipeline(Topology tp, const RenderState &st, const Shader &vs, const Shader &fs) {
  const Shader* sh[] = {&vs,nullptr,nullptr,nullptr,&fs};
  return implPipeline(st,sh,tp);
  }

RenderPipeline Device::pipeline(Topology tp, const RenderState &st, const Shader &vs, const Shader &gs, const Shader &fs) {
  const Shader* sh[] = {&vs,nullptr,nullptr,&gs,&fs};
  return implPipeline(st,sh,tp);
  }

RenderPipeline Device::pipeline(Topology tp, const RenderState &st, const Shader &vs, const Shader &tc, const Shader &te, const Shader &fs) {
  const Shader* sh[] = {&vs,&tc,&te,nullptr,&fs};
  return implPipeline(st,sh,tp);
  }

RenderPipeline Device::implPipeline(const RenderState &st, const Shader* sh[], Topology tp) {
  AbstractGraphicsApi::Shader* shv[5] = {};
  for(size_t i=0; i<5; ++i)
    shv[i] = sh[i]!=nullptr ? sh[i]->impl.handler : nullptr;

  auto ulay = api.createPipelineLayout(dev,shv,5);
  auto pipe = api.createPipeline(dev,st,tp,*ulay.handler,shv,5);
  RenderPipeline f(std::move(pipe),std::move(ulay));
  return f;
  }

RenderPipeline Device::pipeline(const RenderState& st, const Shader& ts, const Shader& ms, const Shader& fs) {
  const Shader*                sh [3] = {&ts,&ms,&fs};
  AbstractGraphicsApi::Shader* shv[3] = {};
  for(size_t i=0; i<3; ++i)
    shv[i] = sh[i]!=nullptr ? sh[i]->impl.handler : nullptr;
  auto ulay = api.createPipelineLayout(dev,shv,3);
  auto pipe = api.createPipeline(dev,st,Topology::Triangles,*ulay.handler,shv,3);
  RenderPipeline f(std::move(pipe),std::move(ulay));
  return f;
  }

CommandBuffer Device::commandBuffer() {
  CommandBuffer buf(*this,api.createCommandBuffer(dev));
  return buf;
  }

const Builtin& Device::builtin() const {
  return builtins;
  }

Detail::VideoBuffer Device::createVideoBuffer(const void *data, size_t count, size_t size, size_t alignedSz, MemUsage usage, BufferHeap flg) {
  Detail::VideoBuffer buf(api.createBuffer(dev,data,count,size,alignedSz,usage,flg),count*alignedSz);
  return  buf;
  }

DescriptorSet Device::descriptors(const PipelineLayout &ulay) {
  if(ulay.impl.handler==nullptr || ulay.impl.handler->descriptorsCount()==0)
    return DescriptorSet(nullptr);
  DescriptorSet ubo(api.createDescriptors(dev,*ulay.impl.handler));
  return ubo;
  }
