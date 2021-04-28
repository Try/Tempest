#include "metalapi.h"

#include <Tempest/Log>
#include <Tempest/Pixmap>

#include "gapi/metal/mtdevice.h"
#include "gapi/metal/mtbuffer.h"
#include "gapi/metal/mtshader.h"
#include "gapi/metal/mtpipeline.h"
#include "gapi/metal/mtcommandbuffer.h"
#include "gapi/metal/mttexture.h"
#include "gapi/metal/mtframebuffer.h"
#include "gapi/metal/mtrenderpass.h"
#include "gapi/metal/mtsync.h"

#import  <Metal/MTLDevice.h>

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

AbstractGraphicsApi::Swapchain *MetalApi::createSwapchain(SystemApi::Window *w, AbstractGraphicsApi::Device *d) {
  return nullptr;
  }

AbstractGraphicsApi::PPass MetalApi::createPass(AbstractGraphicsApi::Device*, const FboMode **att, size_t acount) {
  return PPass(new MtRenderPass(att,acount));
  }

AbstractGraphicsApi::PFbo MetalApi::createFbo(AbstractGraphicsApi::Device *d,
                                              AbstractGraphicsApi::FboLayout *lay,
                                              uint32_t w, uint32_t h, uint8_t clCount,
                                              AbstractGraphicsApi::Swapchain **sx,
                                              AbstractGraphicsApi::Texture **cl,
                                              const uint32_t *imgId,
                                              AbstractGraphicsApi::Texture *zbuf) {
  auto& dx = *reinterpret_cast<MtDevice*>(d);
  auto  zb = reinterpret_cast<MtTexture*>(zbuf);

  MtTexture*   att[256] = {};
  //MtSwapchain* sw[256] = {};
  for(size_t i=0; i<clCount; ++i) {
    att[i] = reinterpret_cast<MtTexture*>(cl[i]);
    //sw [i] = reinterpret_cast<Detail::VSwapchain*>(s[i]);
    }
  return PFbo(new MtFramebuffer(att,clCount,zb));
  }

AbstractGraphicsApi::PFboLayout MetalApi::createFboLayout(AbstractGraphicsApi::Device *d, AbstractGraphicsApi::Swapchain **s, TextureFormat *att, uint8_t attCount) {
  return PFboLayout();
  }

AbstractGraphicsApi::PPipeline MetalApi::createPipeline(AbstractGraphicsApi::Device *d,
                                                        const RenderState &st, size_t stride,
                                                        Topology tp,
                                                        const AbstractGraphicsApi::UniformsLay &ulayImpl,
                                                        const AbstractGraphicsApi::Shader *vs,
                                                        const AbstractGraphicsApi::Shader *tc,
                                                        const AbstractGraphicsApi::Shader *te,
                                                        const AbstractGraphicsApi::Shader *gs,
                                                        const AbstractGraphicsApi::Shader *fs) {
  id<MTLDevice> dx = Detail::get<MtDevice,AbstractGraphicsApi::Device>(d);
  (void)dx;

  auto&         vx = *reinterpret_cast<const MtShader*>(vs);
  auto&         fx = *reinterpret_cast<const MtShader*>(fs);

  return PPipeline(new MtPipeline(st,stride,vx.impl,fx.impl));
  }

AbstractGraphicsApi::PCompPipeline MetalApi::createComputePipeline(AbstractGraphicsApi::Device *d, const AbstractGraphicsApi::UniformsLay &ulayImpl,
                                                                   AbstractGraphicsApi::Shader *sh) {
  return PCompPipeline();
  }

AbstractGraphicsApi::PShader MetalApi::createShader(AbstractGraphicsApi::Device *d, const void *source, size_t src_size) {
  id<MTLDevice> dx  = Detail::get<MtDevice,AbstractGraphicsApi::Device>(d);
  return PShader(new MtShader(dx,source,src_size));
  }

AbstractGraphicsApi::Fence *MetalApi::createFence(AbstractGraphicsApi::Device*) {
  return new MtSync();
  }

AbstractGraphicsApi::Semaphore *MetalApi::createSemaphore(AbstractGraphicsApi::Device *d) {
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
    buf = [dx.impl.get() newBufferWithBytes:mem length:count*alignedSz options:opt];
    return PBuffer(new MtBuffer(dx,buf,opt));
    }

  buf = [dx.impl.get() newBufferWithLength:count*alignedSz options:opt];
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
  id<MTLTexture> tex = tx.impl.get();

  Pixmap::Format  pfrm  = Pixmap::toPixmapFormat(frm);
  size_t          bpp   = Pixmap::bppForFormat(pfrm);
  if(bpp==0)
    throw std::runtime_error("not implemented");

  out = Pixmap(w,h,pfrm);
  [tex getBytes:
    out.data()
    bytesPerRow: w*4
    fromRegion: MTLRegionMake2D(0,0,w,h)
    mipmapLevel: mip
    ];
  }

void MetalApi::readBytes(AbstractGraphicsApi::Device*, AbstractGraphicsApi::Buffer *buf,
                         void *out, size_t size) {
  auto& b = *reinterpret_cast<MtBuffer*>(buf);
  b.read(out,0,size);
  }

AbstractGraphicsApi::Desc *MetalApi::createDescriptors(AbstractGraphicsApi::Device *d, AbstractGraphicsApi::UniformsLay &layP) {
  return nullptr;
  }

AbstractGraphicsApi::PUniformsLay MetalApi::createUboLayout(AbstractGraphicsApi::Device *d,
                                                            const AbstractGraphicsApi::Shader *vs,
                                                            const AbstractGraphicsApi::Shader *tc,
                                                            const AbstractGraphicsApi::Shader *te,
                                                            const AbstractGraphicsApi::Shader *gs,
                                                            const AbstractGraphicsApi::Shader *fs,
                                                            const AbstractGraphicsApi::Shader *cs) {
  return PUniformsLay();
  }

AbstractGraphicsApi::CommandBuffer *MetalApi::createCommandBuffer(AbstractGraphicsApi::Device *d) {
  auto& dx = *reinterpret_cast<MtDevice*>(d);
  return new MtCommandBuffer(dx);
  }

void MetalApi::present(AbstractGraphicsApi::Device *d, AbstractGraphicsApi::Swapchain *sw,
                       uint32_t imageId, const AbstractGraphicsApi::Semaphore *wait) {

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
    id<MTLCommandBuffer> cmd = cx.impl.get();

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
