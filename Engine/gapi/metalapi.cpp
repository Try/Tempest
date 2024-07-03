#if defined(TEMPEST_BUILD_METAL)

#include "metalapi.h"

#if __has_feature(objc_arc)
#error "Objective C++ ARC is not supported"
#endif

#include <Tempest/Log>
#include <Tempest/Pixmap>

#include "gapi/metal/mtdevice.h"
#include "gapi/metal/mtbuffer.h"
#include "gapi/metal/mtshader.h"
#include "gapi/metal/mtpipeline.h"
#include "gapi/metal/mtcommandbuffer.h"
#include "gapi/metal/mttexture.h"
#include "gapi/metal/mtpipelinelay.h"
#include "gapi/metal/mtdescriptorarray.h"
#include "gapi/metal/mtsync.h"
#include "gapi/metal/mtswapchain.h"
#include "gapi/metal/mtaccelerationstructure.h"

#include <Metal/Metal.hpp>

using namespace Tempest;
using namespace Tempest::Detail;

MetalApi::MetalApi(ApiFlags f) {
  if((f & ApiFlags::Validation)==ApiFlags::Validation) {
    setenv("METAL_DEVICE_WRAPPER_TYPE","1",1);
    setenv("METAL_DEBUG_ERROR_MODE",   "5",0);
    setenv("METAL_ERROR_MODE",         "5",0);
    validation = true;
    }
  }

MetalApi::~MetalApi() {
  }

std::vector<AbstractGraphicsApi::Props> MetalApi::devices() const {
#if defined(__OSX__)
  auto dev = MTL::CopyAllDevices();
  try {
    std::vector<AbstractGraphicsApi::Props> p(dev->count());
    for(size_t i=0; i<p.size(); ++i) {
      MtDevice::deductProps(p[i],*reinterpret_cast<MTL::Device*>(dev->object(i)));
      }
    dev->release();
    return p;
    }
  catch(...) {
    dev->release();
    throw;
    }
#else
  std::vector<AbstractGraphicsApi::Props> p(1);
  auto     dev    = NsPtr<MTL::Device>(MTL::CreateSystemDefaultDevice());
  uint32_t mslVer = 0;
  MtDevice::deductProps(p[0],*dev);
  return p;
#endif
  }

AbstractGraphicsApi::Device* MetalApi::createDevice(std::string_view gpuName) {
  return new MtDevice(gpuName,validation);
  }

void MetalApi::destroy(AbstractGraphicsApi::Device *d) {
  delete d;
  }

AbstractGraphicsApi::Swapchain *MetalApi::createSwapchain(SystemApi::Window *w,
                                                          AbstractGraphicsApi::Device* d) {
  auto& dev = *reinterpret_cast<MtDevice*>(d);
  return new MtSwapchain(dev,w);
  }

AbstractGraphicsApi::PPipeline MetalApi::createPipeline(AbstractGraphicsApi::Device *d,
                                                        const RenderState &st,
                                                        Topology tp,
                                                        const AbstractGraphicsApi::PipelineLay &ulayImpl,
                                                        const AbstractGraphicsApi::Shader*const* sh,
                                                        size_t cnt) {
  auto& dx  = *reinterpret_cast<MtDevice*>(d);
  auto& lay = reinterpret_cast<const MtPipelineLay&>(ulayImpl);
  const Detail::MtShader* shader[5] = {};
  for(size_t i=0; i<cnt; ++i)
    shader[i] = reinterpret_cast<const Detail::MtShader*>(sh[i]);
  return PPipeline(new MtPipeline(dx,tp,st,lay, shader,cnt));
  }

AbstractGraphicsApi::PCompPipeline MetalApi::createComputePipeline(AbstractGraphicsApi::Device *d,
                                                                   const AbstractGraphicsApi::PipelineLay& ulayImpl,
                                                                   AbstractGraphicsApi::Shader *cs) {
  auto& dx = *reinterpret_cast<MtDevice*>(d);
  auto& cx = *reinterpret_cast<const MtShader*>(cs);
  auto& lay = reinterpret_cast<const MtPipelineLay&>(ulayImpl);
  return PCompPipeline(new MtCompPipeline(dx,lay,cx));
  }

AbstractGraphicsApi::PShader MetalApi::createShader(AbstractGraphicsApi::Device *d, const void *source, size_t src_size) {
  auto& dx = *reinterpret_cast<MtDevice*>(d);
  return PShader(new MtShader(dx,source,src_size));
  }

AbstractGraphicsApi::Fence *MetalApi::createFence(AbstractGraphicsApi::Device*) {
  return new MtSync();
  }

AbstractGraphicsApi::PBuffer MetalApi::createBuffer(AbstractGraphicsApi::Device *d, const void *mem, size_t size,
                                                    MemUsage usage, BufferHeap flg) {
  auto& dx = *reinterpret_cast<MtDevice*>(d);

  MTL::ResourceOptions opt = 0;
  // https://developer.apple.com/library/archive/documentation/3DDrawing/Conceptual/MTLBestPracticesGuide/ResourceOptions.html#//apple_ref/doc/uid/TP40016642-CH17-SW1
  switch(flg) {
    case BufferHeap::Device:
      opt |= MTL::ResourceStorageModePrivate;
      break;
    case BufferHeap::Upload:
#ifndef __IOS__
      if(size>PAGE_SIZE)
        opt |= MTL::ResourceStorageModeManaged; else
        opt |= MTL::ResourceStorageModeShared;
#else
      opt |= MTL::ResourceStorageModeShared;
#endif
      opt |= MTL::ResourceCPUCacheModeWriteCombined;
      break;
    case BufferHeap::Readback:
      opt |= MTL::ResourceStorageModeManaged;
      opt |= MTL::ResourceCPUCacheModeDefaultCache;
      break;
    }

  return PBuffer(new MtBuffer(dx,mem,size,opt));
  }

AbstractGraphicsApi::PTexture MetalApi::createTexture(AbstractGraphicsApi::Device *d,
                                                      const Pixmap &p, TextureFormat frm, uint32_t mips) {
  auto& dev = *reinterpret_cast<MtDevice*>(d);
  return PTexture(new MtTexture(dev,p,mips,frm));
  }

AbstractGraphicsApi::PTexture MetalApi::createTexture(AbstractGraphicsApi::Device *d,
                                                      const uint32_t w, const uint32_t h, uint32_t mips, TextureFormat frm) {
  auto& dev = *reinterpret_cast<MtDevice*>(d);
  return PTexture(new MtTexture(dev,w,h,1,mips,frm,false));
  }

AbstractGraphicsApi::PTexture MetalApi::createStorage(AbstractGraphicsApi::Device *d,
                                                      const uint32_t w, const uint32_t h, uint32_t mips, TextureFormat frm) {
  auto& dev = *reinterpret_cast<MtDevice*>(d);
  return PTexture(new MtTexture(dev,w,h,1,mips,frm,true));
  }

AbstractGraphicsApi::PTexture MetalApi::createStorage(Device* d,
                                                      const uint32_t w, const uint32_t h, const uint32_t depth, uint32_t mips,
                                                      TextureFormat frm) {
  auto& dev = *reinterpret_cast<MtDevice*>(d);
  return PTexture(new MtTexture(dev,w,h,depth,mips,frm,true));
  }

AbstractGraphicsApi::AccelerationStructure* MetalApi::createBottomAccelerationStruct(Device* d, const RtGeometry* geom, size_t size) {
  auto& dev = *reinterpret_cast<MtDevice*>(d);
  // auto& ix  = *reinterpret_cast<MtBuffer*>(ibo);
  // auto& vx  = *reinterpret_cast<MtBuffer*>(vbo);
  return new MtAccelerationStructure(dev, geom, size);
  }

AbstractGraphicsApi::AccelerationStructure* MetalApi::createTopAccelerationStruct(Device* d, const RtInstance* inst, AccelerationStructure*const* as, size_t size) {
  auto& dev = *reinterpret_cast<MtDevice*>(d);
  return new MtTopAccelerationStructure(dev,inst,as,size);
  }

void MetalApi::readPixels(AbstractGraphicsApi::Device*,
                          Pixmap& out, const AbstractGraphicsApi::PTexture t,
                          TextureFormat frm, const uint32_t w, const uint32_t h, uint32_t mip, bool storageImg) {
  auto& tx = *reinterpret_cast<MtTexture*>(t.handler);
  tx.readPixels(out,frm,w,h,mip);
  }

void MetalApi::readBytes(AbstractGraphicsApi::Device*, AbstractGraphicsApi::Buffer *buf,
                         void *out, size_t size) {
  buf->read(out,0,size);
  }

AbstractGraphicsApi::Desc *MetalApi::createDescriptors(AbstractGraphicsApi::Device* d,
                                                       AbstractGraphicsApi::PipelineLay& layP) {
  auto& dev = *reinterpret_cast<MtDevice*>(d);
  auto& lay = reinterpret_cast<MtPipelineLay&>(layP);
  return new MtDescriptorArray(dev,lay);
  }

AbstractGraphicsApi::PPipelineLay MetalApi::createPipelineLayout(AbstractGraphicsApi::Device*,
                                                                 const AbstractGraphicsApi::Shader*const*sh,
                                                                 size_t cnt) {
  auto bufferSizeBuffer = ShaderReflection::None;
  const std::vector<Detail::ShaderReflection::Binding>* lay[5] = {};

  for(size_t i=0; i<cnt; ++i) {
    if(sh[i]==nullptr)
      continue;
    auto* s = reinterpret_cast<const MtShader*>(sh[i]);
    lay[i] = &s->lay;
    if(s->bufferSizeBuffer) {
      bufferSizeBuffer = ShaderReflection::Stage(bufferSizeBuffer | s->stage);
      }
    }

  return PPipelineLay(new MtPipelineLay(lay,cnt,bufferSizeBuffer));
  }

AbstractGraphicsApi::CommandBuffer *MetalApi::createCommandBuffer(AbstractGraphicsApi::Device *d) {
  auto& dx = *reinterpret_cast<MtDevice*>(d);
  return new MtCommandBuffer(dx);
  }

void MetalApi::present(AbstractGraphicsApi::Device*, AbstractGraphicsApi::Swapchain *sw) {
  auto& s   = *reinterpret_cast<MtSwapchain*>(sw);
  s.present();
  }

void MetalApi::submit(AbstractGraphicsApi::Device *d,
                      AbstractGraphicsApi::CommandBuffer* pcmd,
                      AbstractGraphicsApi::Fence* doneCpu) {
  auto& fence = *reinterpret_cast<MtSync*>(doneCpu);
  fence.signal();

  auto* dx = reinterpret_cast<MtDevice*>(d);
  auto& cx = *reinterpret_cast<MtCommandBuffer*>(pcmd);

  MTL::CommandBuffer& cmd = *cx.impl;
  dx->onSubmit();
  cmd.addCompletedHandler(^(MTL::CommandBuffer* c){
    MTL::CommandBufferStatus s = c->status();
    if(s==MTL::CommandBufferStatusNotEnqueued ||
       s==MTL::CommandBufferStatusEnqueued ||
       s==MTL::CommandBufferStatusCommitted ||
       s==MTL::CommandBufferStatusScheduled)
      return;

    if(s==MTL::CommandBufferStatusCompleted)
      fence.reset(); else
      fence.reset(s, MTL::CommandBufferError(c->error()->code()), c->error());
    dx->onFinish();
    });
  cmd.commit();
  }

void MetalApi::getCaps(AbstractGraphicsApi::Device *d, AbstractGraphicsApi::Props &caps) {
  auto& dx = *reinterpret_cast<MtDevice*>(d);
  caps = dx.prop;
  }

#endif

