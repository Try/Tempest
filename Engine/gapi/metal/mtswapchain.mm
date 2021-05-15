#include "mtswapchain.h"

#include <Tempest/Application>
#include <Tempest/Except>
#include <Tempest/Log>

#include "mtdevice.h"

#import <QuartzCore/CAMetalLayer.h>
#import <Metal/MTLTexture.h>
#import <Metal/MTLCommandQueue.h>

using namespace Tempest;
using namespace Tempest::Detail;

@interface MetalView : NSView
@end

@implementation MetalView
+ (id)layerClass {
  return [CAMetalLayer class];
  }

- (CALayer *)makeBackingLayer {
  return [CAMetalLayer layer];
  }
@end

// note : MoltenVK supports NSView, UIView, CAMetalLayer, so we should align to it
MtSwapchain::MtSwapchain(MtDevice& dev, NSWindow *w)
  :dev(dev), wnd(w) {
  NSRect rect = [wnd frame];
  sz = {int(rect.size.width), int(rect.size.height)};

  view = [[MetalView alloc] initWithFrame:rect];
  view.wantsLayer = YES;
  wnd.contentView = view;

  CAMetalLayer* lay = reinterpret_cast<CAMetalLayer*>(wnd.contentView.layer);
  lay.device = dev.impl;

  const float dpi = [NSScreen mainScreen].backingScaleFactor;
  [lay setContentsScale:dpi];

  //lay.maximumDrawableCount      = 2;
  lay.pixelFormat               = MTLPixelFormatBGRA8Unorm;
  lay.allowsNextDrawableTimeout = NO;
  lay.framebufferOnly           = NO;

  reset();
  }

MtSwapchain::~MtSwapchain() {
  releaseTex();
  if(view!=nil)
    [view release];
  }

void MtSwapchain::reset() {
  std::lock_guard<SpinLock> guard(sync);
  // https://developer.apple.com/documentation/quartzcore/cametallayer?language=objc
  releaseTex();

  CAMetalLayer* lay = reinterpret_cast<CAMetalLayer*>(wnd.contentView.layer);

  NSRect wrect = [wnd convertRectToBacking:[wnd frame]];
  NSRect lrect = lay.frame;
  if(wrect.size.width!=lrect.size.width || wrect.size.height!=lrect.size.height) {
    // TODO:screen.backingScaleFactor
    lay.drawableSize = wrect.size;
    sz = {int(wrect.size.width), int(wrect.size.height)};
    }
  imgCount = 2; //lay.maximumDrawableCount;

  @autoreleasepool {
    img.resize(imgCount);
    for(size_t i=0; i<imgCount; ++i)
      img[i].tex = mkTexture();
    }
  }

uint32_t MtSwapchain::nextImage(AbstractGraphicsApi::Semaphore*) {
  for(;;) {
    std::lock_guard<SpinLock> guard(sync);
    for(size_t i=0; i<img.size(); ++i) {
      if(img[i].inUse)
        continue;
      img[i].inUse = true;
      return i;
      }
    }
  throw SwapchainSuboptimal();
  }

void MtSwapchain::present(uint32_t i) {
  CAMetalLayer* lay = reinterpret_cast<CAMetalLayer*>(wnd.contentView.layer);

  @autoreleasepool {
    id<CAMetalDrawable>       drawable = [lay nextDrawable];
    id<MTLCommandBuffer>      cmd      = [dev.queue commandBuffer];
    id<MTLBlitCommandEncoder> blit     = [cmd blitCommandEncoder];

    id<MTLTexture> dr = drawable.texture;
    if(dr.width!=img[i].tex.width || dr.height!=img[i].tex.height)
      throw SwapchainSuboptimal();

    std::lock_guard<SpinLock> guard(sync);
    [blit copyFromTexture:img[i].tex
                          sourceSlice:0
                          sourceLevel:0
                          toTexture:dr
                          destinationSlice:0
                          destinationLevel:0
                          sliceCount:1
                          levelCount:1];
    [blit endEncoding];

    [cmd presentDrawable:drawable];
    [cmd addCompletedHandler:^(id<MTLCommandBuffer>)  {
      std::lock_guard<SpinLock> guard(sync);
      img[i].inUse = false;
      }];
    [cmd commit];
    }
  }

void MtSwapchain::releaseTex() {
  for(size_t i=0; i<img.size(); ++i) {
    [img[i].tex release];
    }
  }

id<MTLTexture> MtSwapchain::mkTexture() {
  MTLTextureDescriptor* desc = [MTLTextureDescriptor new];
  if(desc==nil)
    throw std::system_error(GraphicsErrc::OutOfVideoMemory);

  desc.textureType      = MTLTextureType2D;
  desc.pixelFormat      = MTLPixelFormatBGRA8Unorm;
  desc.width            = sz.w;
  desc.height           = sz.h;
  desc.mipmapLevelCount = 1;

  desc.cpuCacheMode = MTLCPUCacheModeDefaultCache;
  desc.storageMode  = MTLStorageModePrivate;
  desc.usage        = MTLTextureUsageRenderTarget;

  desc.allowGPUOptimizedContents = YES;

  auto impl = [dev.impl newTextureWithDescriptor:desc];
  if(impl==nil)
    throw std::system_error(GraphicsErrc::OutOfVideoMemory);
  return impl;
  }

uint32_t MtSwapchain::imageCount() const {
  return imgCount;
  }

uint32_t MtSwapchain::w() const {
  return sz.w;
  }

uint32_t MtSwapchain::h() const {
  return sz.h;
  }

MTLPixelFormat MtSwapchain::format() const {
  CAMetalLayer* lay = reinterpret_cast<CAMetalLayer*>(wnd.contentView.layer);
  return lay.pixelFormat;
  }
