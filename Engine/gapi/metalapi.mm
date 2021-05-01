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
#include "gapi/metal/mtfbolayout.h"
#include "gapi/metal/mtframebuffer.h"
#include "gapi/metal/mtrenderpass.h"
#include "gapi/metal/mtpipelinelay.h"
#include "gapi/metal/mtdescriptorarray.h"
#include "gapi/metal/mtsync.h"
#include "gapi/metal/mtswapchain.h"

#import  <Metal/MTLDevice.h>
#import  <Metal/MTLCommandQueue.h>
#import  <AppKit/AppKit.h>

using namespace Tempest;
using namespace Tempest::Detail;

struct MetalApi::Impl {

  };

MetalApi::MetalApi(ApiFlags) {
  impl.reset(new Impl());
  }

MetalApi::~MetalApi() {
  }

std::vector<AbstractGraphicsApi::Props> MetalApi::devices() const {
  return {};
  }

AbstractGraphicsApi::Device *MetalApi::createDevice(const char *gpuName) {
  return new MtDevice();
  }

void MetalApi::destroy(AbstractGraphicsApi::Device *d) {
  delete d;
  }

AbstractGraphicsApi::Swapchain *MetalApi::createSwapchain(SystemApi::Window *w,
                                                          AbstractGraphicsApi::Device* d) {
  auto& dev = *reinterpret_cast<MtDevice*>(d);

  NSObject* obj = reinterpret_cast<NSObject*>(w);
  if([obj isKindOfClass : [NSWindow class]])
    return new MtSwapchain(dev,reinterpret_cast<NSWindow*>(w));
  return nullptr;
  }

AbstractGraphicsApi::PPass MetalApi::createPass(AbstractGraphicsApi::Device*, const FboMode **att, size_t acount) {
  return PPass(new MtRenderPass(att,acount));
  }

AbstractGraphicsApi::PFbo MetalApi::createFbo(AbstractGraphicsApi::Device*,
                                              AbstractGraphicsApi::FboLayout *lay,
                                              uint32_t w, uint32_t h, uint8_t clCount,
                                              AbstractGraphicsApi::Swapchain **sx,
                                              AbstractGraphicsApi::Texture **cl,
                                              const uint32_t *imgId,
                                              AbstractGraphicsApi::Texture *zbuf) {
  auto& lx = *reinterpret_cast<MtFboLayout*>(lay);
  auto  zb = reinterpret_cast<MtTexture*>(zbuf);

  MtTexture*   att[256] = {};
  MtSwapchain* sw [256] = {};
  for(size_t i=0; i<clCount; ++i) {
    att[i] = reinterpret_cast<MtTexture*>(cl[i]);
    sw [i] = reinterpret_cast<MtSwapchain*>(sx[i]);
    }
  return PFbo(new MtFramebuffer(lx,sw,imgId,att,clCount,zb));
  }

AbstractGraphicsApi::PFboLayout MetalApi::createFboLayout(AbstractGraphicsApi::Device*, AbstractGraphicsApi::Swapchain **s,
                                                          TextureFormat *att, uint8_t attCount) {
  auto sw = reinterpret_cast<MtSwapchain**>(s);
  return PFboLayout(new MtFboLayout(sw,att,attCount));
  }

AbstractGraphicsApi::PPipeline MetalApi::createPipeline(AbstractGraphicsApi::Device *d,
                                                        const RenderState &st, size_t stride,
                                                        Topology tp,
                                                        const AbstractGraphicsApi::PipelineLay &ulayImpl,
                                                        const AbstractGraphicsApi::Shader *vs,
                                                        const AbstractGraphicsApi::Shader *tc,
                                                        const AbstractGraphicsApi::Shader *te,
                                                        const AbstractGraphicsApi::Shader *gs,
                                                        const AbstractGraphicsApi::Shader *fs) {
  auto& dx = *reinterpret_cast<MtDevice*>(d);

  auto& vx = *reinterpret_cast<const MtShader*>(vs);
  auto& fx = *reinterpret_cast<const MtShader*>(fs);

  return PPipeline(new MtPipeline(dx,tp,st,stride,vx,fx));
  }

AbstractGraphicsApi::PCompPipeline MetalApi::createComputePipeline(AbstractGraphicsApi::Device *d, const AbstractGraphicsApi::PipelineLay &ulayImpl,
                                                                   AbstractGraphicsApi::Shader *cs) {
  auto& dx = *reinterpret_cast<MtDevice*>(d);
  auto& cx = *reinterpret_cast<const MtShader*>(cs);
  return PCompPipeline(new MtCompPipeline(dx,cx));
  }

AbstractGraphicsApi::PShader MetalApi::createShader(AbstractGraphicsApi::Device *d, const void *source, size_t src_size) {
  auto& dx = *reinterpret_cast<MtDevice*>(d);
  return PShader(new MtShader(dx,source,src_size));
  }

AbstractGraphicsApi::Fence *MetalApi::createFence(AbstractGraphicsApi::Device*) {
  return new MtSync();
  }

AbstractGraphicsApi::Semaphore *MetalApi::createSemaphore(AbstractGraphicsApi::Device*) {
  return nullptr;
  }

AbstractGraphicsApi::PBuffer MetalApi::createBuffer(AbstractGraphicsApi::Device *d,
                                                    const void *mem, size_t count, size_t size, size_t alignedSz,
                                                    MemUsage /*usage*/, BufferHeap flg) {
  auto& dx = *reinterpret_cast<MtDevice*>(d);

  MTLResourceOptions opt = 0;
  // https://developer.apple.com/library/archive/documentation/3DDrawing/Conceptual/MTLBestPracticesGuide/ResourceOptions.html#//apple_ref/doc/uid/TP40016642-CH17-SW1
  switch(flg) {
    case BufferHeap::Device:
      opt |= MTLResourceStorageModePrivate;
      break;
    case BufferHeap::Upload:
#ifndef __IOS__
      if(count*alignedSz>PAGE_SIZE)
        opt |= MTLResourceStorageModeManaged; else
        opt |= MTLResourceStorageModeShared;
#else
      opt |= MTLResourceStorageModeShared;
#endif
      opt |= MTLResourceCPUCacheModeWriteCombined;
      break;
    case BufferHeap::Readback:
      opt |= MTLResourceStorageModeManaged;
      opt |= MTLResourceCPUCacheModeDefaultCache;
      break;
    }

  opt |= MTLResourceHazardTrackingModeDefault;

  id<MTLBuffer> buf;
  if(alignedSz==size && 0==(opt & MTLResourceStorageModePrivate)) {
    buf = [dx.impl newBufferWithBytes:mem length:count*alignedSz options:opt];
    return PBuffer(new MtBuffer(dx,buf,opt));
    }

  buf = [dx.impl newBufferWithLength:count*alignedSz options:opt];
  auto ret = PBuffer(new MtBuffer(dx,buf,opt));
  ret.handler->update(mem,0,count,size,alignedSz);
  return ret;
  }

AbstractGraphicsApi::PTexture MetalApi::createTexture(AbstractGraphicsApi::Device *d,
                                                      const Pixmap &p, TextureFormat frm, uint32_t mips) {
  return PTexture();
  }

AbstractGraphicsApi::PTexture MetalApi::createTexture(AbstractGraphicsApi::Device *d,
                                                      const uint32_t w, const uint32_t h, uint32_t mips, TextureFormat frm) {
  auto& dev = *reinterpret_cast<MtDevice*>(d);
  return PTexture(new MtTexture(dev,w,h,mips,frm));
  }

AbstractGraphicsApi::PTexture MetalApi::createStorage(AbstractGraphicsApi::Device *d,
                                                      const uint32_t w, const uint32_t h, uint32_t mips, TextureFormat frm) {
  return PTexture();
  }

void MetalApi::readPixels(AbstractGraphicsApi::Device *d,
                          Pixmap &out, const AbstractGraphicsApi::PTexture t,
                          TextureLayout /*lay*/, TextureFormat frm,
                          const uint32_t w, const uint32_t h, uint32_t mip) {
  auto&          tx  = *reinterpret_cast<MtTexture*>(t.handler);
  id<MTLTexture> tex = tx.impl;

  Pixmap::Format  pfrm  = Pixmap::toPixmapFormat(frm);
  size_t          bpp   = Pixmap::bppForFormat(pfrm);
  if(bpp==0)
    throw std::runtime_error("not implemented");

  out = Pixmap(w,h,pfrm);
  [tex getBytes: out.data()
    bytesPerRow: w*bpp
    fromRegion : MTLRegionMake2D(0,0,w,h)
    mipmapLevel: mip
    ];
  }

void MetalApi::readBytes(AbstractGraphicsApi::Device*, AbstractGraphicsApi::Buffer *buf,
                         void *out, size_t size) {
  auto& b = *reinterpret_cast<MtBuffer*>(buf);
  b.read(out,0,size);
  }

AbstractGraphicsApi::Desc *MetalApi::createDescriptors(AbstractGraphicsApi::Device*,
                                                       AbstractGraphicsApi::PipelineLay& layP) {
  auto& lay = reinterpret_cast<MtPipelineLay&>(layP);
  return new MtDescriptorArray(lay);
  }

AbstractGraphicsApi::PPipelineLay MetalApi::createPipelineLayout(AbstractGraphicsApi::Device*,
                                                                 const AbstractGraphicsApi::Shader *vs,
                                                                 const AbstractGraphicsApi::Shader *tc,
                                                                 const AbstractGraphicsApi::Shader *te,
                                                                 const AbstractGraphicsApi::Shader *gs,
                                                                 const AbstractGraphicsApi::Shader *fs,
                                                                 const AbstractGraphicsApi::Shader *cs) {
  //auto& dx = *reinterpret_cast<MtDevice*>(d);
  if(cs!=nullptr) {
    auto* comp = reinterpret_cast<const Detail::MtShader*>(cs);
    return PPipelineLay(new MtPipelineLay(comp->lay));
    }

  const Shader* sh[] = {vs,tc,te,gs,fs};
  const std::vector<Detail::ShaderReflection::Binding>* lay[5] = {};
  for(size_t i=0; i<5; ++i) {
    if(sh[i]==nullptr)
      continue;
    auto* s = reinterpret_cast<const MtShader*>(sh[i]);
    lay[i] = &s->lay;
    }
  return PPipelineLay(new MtPipelineLay(lay,5));
  }

AbstractGraphicsApi::CommandBuffer *MetalApi::createCommandBuffer(AbstractGraphicsApi::Device *d) {
  auto& dx = *reinterpret_cast<MtDevice*>(d);
  return new MtCommandBuffer(dx);
  }

void MetalApi::present(AbstractGraphicsApi::Device *d,
                       AbstractGraphicsApi::Swapchain *sw,
                       uint32_t /*imageId*/,
                       const AbstractGraphicsApi::Semaphore*) {
  auto& dev = *reinterpret_cast<MtDevice*>(d);
  auto& s   = *reinterpret_cast<MtSwapchain*>(sw);

  id<MTLCommandBuffer> cmd = [dev.queue commandBuffer];
  [cmd presentDrawable:s.current];
  [cmd addCompletedHandler:^(id<MTLCommandBuffer> c)  {
    [c release];
    }];
  [cmd commit];
  s.releaseImg();
  }

void MetalApi::submit(AbstractGraphicsApi::Device *d,
                      AbstractGraphicsApi::CommandBuffer *cmd,
                      AbstractGraphicsApi::Semaphore *wait,
                      AbstractGraphicsApi::Semaphore *done,
                      AbstractGraphicsApi::Fence *doneCpu) {
  this->submit(d,&cmd,1,&wait,1,&done,1,doneCpu);
  }

void MetalApi::submit(AbstractGraphicsApi::Device*,
                      AbstractGraphicsApi::CommandBuffer **pcmd, size_t count,
                      AbstractGraphicsApi::Semaphore **wait, size_t waitCnt,
                      AbstractGraphicsApi::Semaphore **done, size_t doneCnt,
                      AbstractGraphicsApi::Fence *doneCpu) {
  auto& fence = *reinterpret_cast<MtSync*>(doneCpu);
  fence.signal();
  for(size_t i=0; i<count; ++i) {
    auto& cx = *reinterpret_cast<MtCommandBuffer*>(pcmd[i]);
    id<MTLCommandBuffer> cmd = cx.impl;

    [cmd addCompletedHandler:^(id<MTLCommandBuffer> c) {
      (void)c;
      fence.reset();
      }];
    [cmd commit];
    [cmd waitUntilCompleted];
    }
  }

void MetalApi::getCaps(AbstractGraphicsApi::Device *d, AbstractGraphicsApi::Props &caps) {
  auto& dx = *reinterpret_cast<MtDevice*>(d);
  caps = dx.prop;
  }
