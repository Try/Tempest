#include "device.h"
#include "utility/smallarray.h"

#include <Tempest/Fence>
#include <Tempest/PipelineLayout>
#include <Tempest/UniformBuffer>
#include <Tempest/File>
#include <Tempest/Pixmap>
#include <Tempest/Except>

#include <string>
#include <cassert>

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

static uint32_t mipCount(uint32_t w, uint32_t h, uint32_t d) {
  uint32_t s = std::max(std::max(w,h),d);
  uint32_t n = 1;
  while(s>1) {
    ++n;
    s = s/2;
    }
  return n;
  }

Device::Impl::Impl(AbstractGraphicsApi &api, std::string_view name)
  :api(api) {
  dev=api.createDevice(name);
  }

Device::Impl::Impl(AbstractGraphicsApi& api, DeviceType type)
  :api(api) {
  if(type==DeviceType::Unknown) {
    dev=api.createDevice("");
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
  delete dev;
  }

Device::Device(AbstractGraphicsApi& api)
  :Device(api,""){
  }

Device::Device(AbstractGraphicsApi &api, std::string_view name)
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

DescriptorArray Device::descriptors(const std::vector<const StorageBuffer *> &buf) {
  return descriptors(buf.data(), buf.size());
  }

DescriptorArray Device::descriptors(const StorageBuffer * const *buf, size_t size) {
  if(!devProps.descriptors.nonUniformIndexing)
    throw std::system_error(Tempest::GraphicsErrc::UnsupportedExtension, "nonUniformIndexing");
  Detail::SmallArray<AbstractGraphicsApi::Buffer*,32> arr(size);
  for(size_t i=0; i<size; ++i)
    arr[i] = buf[i] ? buf[i]->impl.impl.handler : nullptr;
  DescriptorArray d(*this,api.createDescriptors(dev, arr.get(), size));
  return d;
  }

DescriptorArray Device::descriptors(const std::vector<const Texture2d *> &tex) {
  return descriptors(tex.data(), tex.size());
  }

DescriptorArray Device::descriptors(const Texture2d * const *tex, size_t size) {
  if(!devProps.descriptors.nonUniformIndexing)
    throw std::system_error(Tempest::GraphicsErrc::UnsupportedExtension, "nonUniformIndexing");
  Detail::SmallArray<AbstractGraphicsApi::Texture*,32> arr(size);
  for(size_t i=0; i<size; ++i)
    arr[i] = tex[i] ? tex[i]->impl.handler : nullptr;
  DescriptorArray d(*this,api.createDescriptors(dev, arr.get(), size, uint32_t(-1)));
  return d;
  }

DescriptorArray Device::descriptors(const std::vector<const Texture2d *> &tex, const Sampler &smp) {
  return descriptors(tex.data(), tex.size(), smp);
  }

DescriptorArray Device::descriptors(const Texture2d * const *tex, size_t size, const Sampler &smp) {
  if(!devProps.descriptors.nonUniformIndexing)
    throw std::system_error(Tempest::GraphicsErrc::UnsupportedExtension, "nonUniformIndexing");
  Detail::SmallArray<AbstractGraphicsApi::Texture*,32> arr(size);
  for(size_t i=0; i<size; ++i)
    arr[i] = tex[i] ? tex[i]->impl.handler : nullptr;
  DescriptorArray d(*this,api.createDescriptors(dev, arr.get(), size, uint32_t(-1), smp));
  return d;
  }

Swapchain Device::swapchain(SystemApi::Window* w) const {
  return Swapchain(api.createSwapchain(w,impl.dev));
  }

const Device::Props& Device::properties() const {
  return devProps;
  }

Attachment Device::attachment(TextureFormat frm, const uint32_t w, const uint32_t h, const bool mips) {
  if(!devProps.hasSamplerFormat(frm) && !devProps.hasAttachFormat(frm))
    throw std::system_error(Tempest::GraphicsErrc::UnsupportedTextureFormat, formatName(frm));
  if(w>devProps.tex2d.maxSize || h>devProps.tex2d.maxSize)
    throw std::system_error(Tempest::GraphicsErrc::TooLargeTexture, std::to_string(std::max(w,h)));
  uint32_t mipCnt = mips ? mipCount(w,h) : 1;
  Texture2d t(*this,api.createTexture(dev,w,h,mipCnt,frm),w,h,1,frm);
  return Attachment(std::move(t));
  }

ZBuffer Device::zbuffer(TextureFormat frm, const uint32_t w, const uint32_t h) {
  if(!devProps.hasDepthFormat(frm))
    throw std::system_error(Tempest::GraphicsErrc::UnsupportedTextureFormat, formatName(frm));
  if(w>devProps.tex2d.maxSize || h>devProps.tex2d.maxSize)
    throw std::system_error(Tempest::GraphicsErrc::TooLargeTexture, std::to_string(std::max(w,h)));
  Texture2d t(*this,api.createTexture(dev,w,h,1,frm),w,h,1,frm);
  return ZBuffer(std::move(t),!devProps.hasSamplerFormat(frm));
  }

Texture2d Device::texture(const Pixmap &pm, const bool mips) {
  TextureFormat format = pm.format();
  uint32_t      mipCnt = mips ? mipCount(pm.w(),pm.h()) : 1;
  const Pixmap* p=&pm;
  Pixmap        alt;

  if(pm.w()>devProps.tex2d.maxSize || pm.h()>devProps.tex2d.maxSize)
    throw std::system_error(Tempest::GraphicsErrc::TooLargeTexture, std::to_string(std::max(pm.w(),pm.h())));

  if(isCompressedFormat(format)){
    if(devProps.hasSamplerFormat(format) && (!mips || pm.mipCount()>1)){
      mipCnt = pm.mipCount();
      } else {
      alt    = Pixmap(pm,TextureFormat::RGBA8);
      p      = &alt;
      format = TextureFormat::RGBA8;
      }
    }

  if(!devProps.hasSamplerFormat(format)) {
    if(format==TextureFormat::RGB8){
      alt    = Pixmap(pm,TextureFormat::RGBA8);
      p      = &alt;
      format = TextureFormat::RGBA8;
      }
    else if(format==TextureFormat::RGB16){
      alt    = Pixmap(pm,TextureFormat::RGBA16);
      p      = &alt;
      format = TextureFormat::RGBA16;
      }
    else if(format==TextureFormat::RGB32F){
      alt    = Pixmap(pm,TextureFormat::RGBA32F);
      p      = &alt;
      format = TextureFormat::RGBA32F;
      }
    else {
      throw std::system_error(Tempest::GraphicsErrc::UnsupportedTextureFormat, formatName(format));
      }
    }

  Texture2d t(*this,api.createTexture(dev,*p,format,mipCnt),p->w(),p->h(),1,format);
  return t;
  }

StorageImage Device::image2d(TextureFormat frm, const uint32_t w, const uint32_t h, const bool mips) {
  if(!devProps.hasStorageFormat(frm))
    throw std::system_error(Tempest::GraphicsErrc::UnsupportedTextureFormat, formatName(frm));
  if(w>devProps.tex2d.maxSize || h>devProps.tex2d.maxSize)
    throw std::system_error(Tempest::GraphicsErrc::UnsupportedTextureFormat, formatName(frm));
  uint32_t mipCnt = mips ? mipCount(w,h) : 1;
  Texture2d t(*this,api.createStorage(dev,w,h,mipCnt,frm),w,h,1,frm);
  return StorageImage(std::move(t));
  }

StorageImage Device::image3d(TextureFormat frm, const uint32_t w, const uint32_t h, const uint32_t d, const bool mips) {
  if(!devProps.hasStorageFormat(frm))
    throw std::system_error(Tempest::GraphicsErrc::UnsupportedTextureFormat, formatName(frm));
  if(w>devProps.tex3d.maxSize || h>devProps.tex3d.maxSize || d>devProps.tex3d.maxSize)
    throw std::system_error(Tempest::GraphicsErrc::UnsupportedTextureFormat, formatName(frm));
  uint32_t mipCnt = mips ? mipCount(w,h,d) : 1;
  Texture2d t(*this,api.createStorage(dev,w,h,d,mipCnt,frm),w,h,d,frm);
  return StorageImage(std::move(t));
  }

AccelerationStructure Device::blas(const std::vector<RtGeometry>& geom) {
  return blas(geom.data(), geom.size());
  }

AccelerationStructure Device::blas(std::initializer_list<RtGeometry> geom) {
  return blas(geom.begin(), geom.size());
  }

AccelerationStructure Device::blas(const RtGeometry* geom, size_t geomSize) {
  if(!properties().raytracing.rayQuery)
    throw std::system_error(Tempest::GraphicsErrc::UnsupportedExtension, "rayQuery");
  if(geomSize==0)
    return AccelerationStructure();

  Detail::SmallArray<AbstractGraphicsApi::RtGeometry,32> g(geomSize);
  for(size_t i=0; i<geomSize; ++i) {
    const uint32_t stride = uint32_t(geom[i].vboStride);
    assert(3*sizeof(float)<=stride); // float3 positions, no overlap

    auto& gx = g[i];
    gx.vbo     = geom[i].vbo->impl.impl.handler;
    gx.vboSz   = geom[i].vbo->byteSize()/stride;
    gx.stride  = geom[i].vboStride;
    gx.ibo     = geom[i].ibo->impl.impl.handler;
    gx.iboSz   = geom[i].iboSize;
    gx.ioffset = geom[i].iboOffset;
    gx.icls    = geom[i].icls;
    }
  auto blas = api.createBottomAccelerationStruct(dev, g.get(), geomSize);
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
  size_t nonEmptyGeomSize = 0;
  for(size_t i=0; i<geomSize; ++i) {
    if(geom[i].blas->impl.handler==nullptr)
      continue;
    as[nonEmptyGeomSize] = geom[i].blas->impl.handler;
    ++nonEmptyGeomSize;
    }
  auto tlas = api.createTopAccelerationStruct(dev,geom,as.data(),nonEmptyGeomSize);
  return AccelerationStructure(*this,tlas);
  }

Pixmap Device::readPixels(const Texture2d &t, uint32_t mip) {
  Pixmap pm;
  api.readPixels(dev,pm,t.impl,t.format(),uint32_t(t.w()),uint32_t(t.h()),mip,false);
  return pm;
  }

Pixmap Device::readPixels(const Attachment& t, uint32_t mip) {
  Pixmap pm;
  auto& tx = textureCast<const Texture2d&>(t);
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

Detail::VideoBuffer Device::createVideoBuffer(const void *data, size_t size, MemUsage usage, BufferHeap flg) {
  Detail::VideoBuffer buf(api.createBuffer(dev,data,size,usage,flg), size);
  return  buf;
  }
