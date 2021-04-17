#include "metalapi.h"

#include "gapi/metal/mtdevice.h"
#include "gapi/metal/mtbuffer.h"
#include "gapi/metal/mtshader.h"
#include "gapi/metal/mtpipeline.h"

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

AbstractGraphicsApi::PPass MetalApi::createPass(AbstractGraphicsApi::Device *d, const FboMode **att, size_t acount) {
  return PPass();
  }

AbstractGraphicsApi::PFbo MetalApi::createFbo(AbstractGraphicsApi::Device *d,
                                              AbstractGraphicsApi::FboLayout *lay, uint32_t w, uint32_t h, uint8_t clCount,
                                              AbstractGraphicsApi::Swapchain **sx,
                                              AbstractGraphicsApi::Texture **tcl,
                                              const uint32_t *imgId,
                                              AbstractGraphicsApi::Texture *zbuf) {
  return PFbo();
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

AbstractGraphicsApi::Fence *MetalApi::createFence(AbstractGraphicsApi::Device *d) {
  return nullptr;
  }

AbstractGraphicsApi::Semaphore *MetalApi::createSemaphore(AbstractGraphicsApi::Device *d) {
  return nullptr;
  }

AbstractGraphicsApi::PBuffer MetalApi::createBuffer(AbstractGraphicsApi::Device *d,
                                                    const void *mem, size_t count, size_t size, size_t alignedSz,
                                                    MemUsage usage, BufferHeap flg) {
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

  id<MTLBuffer> ret = [dx.impl.get() newBufferWithLength:count*alignedSz options:opt];
  return PBuffer(new MtBuffer(dx,ret,opt));
  }

AbstractGraphicsApi::PTexture MetalApi::createTexture(AbstractGraphicsApi::Device *d,
                                                      const Pixmap &p, TextureFormat frm, uint32_t mips) {
  return PTexture();
  }

AbstractGraphicsApi::PTexture MetalApi::createTexture(AbstractGraphicsApi::Device *d,
                                                      const uint32_t w, const uint32_t h, uint32_t mips, TextureFormat frm) {
  return PTexture();
  }

AbstractGraphicsApi::PTexture MetalApi::createStorage(AbstractGraphicsApi::Device *d,
                                                      const uint32_t w, const uint32_t h, uint32_t mips, TextureFormat frm) {
  return PTexture();
  }

void MetalApi::readPixels(AbstractGraphicsApi::Device *d,
                          Pixmap &out, const AbstractGraphicsApi::PTexture t, TextureLayout lay, TextureFormat frm,
                          const uint32_t w, const uint32_t h, uint32_t mip) {

  }

void MetalApi::readBytes(AbstractGraphicsApi::Device *d, AbstractGraphicsApi::Buffer *buf, void *out, size_t size) {

  }

AbstractGraphicsApi::Desc *MetalApi::createDescriptors(AbstractGraphicsApi::Device *d, AbstractGraphicsApi::UniformsLay &layP) {
  return nullptr;
  }

AbstractGraphicsApi::PUniformsLay MetalApi::createUboLayout(AbstractGraphicsApi::Device *d, const AbstractGraphicsApi::Shader *vs, const AbstractGraphicsApi::Shader *tc, const AbstractGraphicsApi::Shader *te, const AbstractGraphicsApi::Shader *gs, const AbstractGraphicsApi::Shader *fs, const AbstractGraphicsApi::Shader *cs) {
  return PUniformsLay();
  }

AbstractGraphicsApi::CommandBuffer *MetalApi::createCommandBuffer(AbstractGraphicsApi::Device *d) {
  return nullptr;
  }

void MetalApi::present(AbstractGraphicsApi::Device *d, AbstractGraphicsApi::Swapchain *sw,
                       uint32_t imageId, const AbstractGraphicsApi::Semaphore *wait) {

  }

void MetalApi::submit(AbstractGraphicsApi::Device *d, AbstractGraphicsApi::CommandBuffer *cmd,
                      AbstractGraphicsApi::Semaphore *wait, AbstractGraphicsApi::Semaphore *onReady, AbstractGraphicsApi::Fence *doneCpu) {

  }

void MetalApi::submit(AbstractGraphicsApi::Device *d,
                      AbstractGraphicsApi::CommandBuffer **cmd, size_t count,
                      AbstractGraphicsApi::Semaphore **wait, size_t waitCnt,
                      AbstractGraphicsApi::Semaphore **done, size_t doneCnt,
                      AbstractGraphicsApi::Fence *doneCpu) {

  }

void MetalApi::getCaps(AbstractGraphicsApi::Device *d, AbstractGraphicsApi::Props &caps) {

  }
