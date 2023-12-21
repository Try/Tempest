#if defined(TEMPEST_BUILD_METAL)

#include "mtswapchain.h"

#include <Tempest/Application>
#include <Tempest/Except>
#include <Tempest/Log>

#include "mtdevice.h"

#ifdef __OSX__
#import <AppKit/AppKit.h>
#endif

#ifdef __IOS__
#import <UIKit/UIKit.h>
#import <Foundation/Foundation.h>
#endif

#import <QuartzCore/QuartzCore.hpp>
#import <QuartzCore/CAMetalLayer.h>
#import <Metal/MTLTexture.h>
#import <Metal/MTLCommandQueue.h>

using namespace Tempest;
using namespace Tempest::Detail;

#ifdef __OSX__
using SysView = NSView;
using SysWindow = NSWindow;
#endif

#ifdef __IOS__
using SysView = UIView;
using SysWindow = UIWindow;
#endif

@class MetalView;

@interface MetalView : SysView
@end

@implementation MetalView
+ (id)layerClass {
  return [CAMetalLayer class];
  }

- (CALayer *)makeBackingLayer {
  return [CAMetalLayer layer];
  }
@end

struct MtSwapchain::Impl {
  SysWindow* wnd  = nil;
  MetalView* view = nil;

  CAMetalLayer* metalLayer() {
#if defined(__OSX__)
    return reinterpret_cast<CAMetalLayer*>(wnd.contentView.layer);
#elif defined(__IOS__)
    return reinterpret_cast<CAMetalLayer*>(wnd.rootViewController.view.layer);
#endif
    }
  };

static float backingScaleFactor(SysWindow* w) {
#if defined(__OSX__)
  return [w screen].backingScaleFactor;
#elif defined(__IOS__)
  return [UIScreen mainScreen].scale;
#endif
  }

#if defined(__OSX__)
static NSRect windowRect(NSWindow* wnd) {
  NSRect fr = [wnd contentRectForFrameRect:[wnd frame]];
  fr = [wnd convertRectToBacking:fr];
  return fr;
  }
#elif defined(__IOS__)
static CGRect windowRect(UIWindow* wnd) {
  CGRect  fr    = wnd.rootViewController.view.frame;
  CGFloat scale = wnd.contentScaleFactor;
  // fr = [wnd convertRect:fr fromView:wnd.rootViewController.view];
  
  fr.origin.x    *= scale;
  fr.origin.y    *= scale;
  fr.size.width  *= scale;
  fr.size.height *= scale;
  return fr;
  }
#endif

// note : MoltenVK supports NSView, UIView, CAMetalLayer, so we should align to it
MtSwapchain::MtSwapchain(MtDevice& dev, SystemApi::Window *w)
  :dev(dev), pimpl(new Impl()) {
  NSObject* obj = reinterpret_cast<NSObject*>(w);
  if([obj isKindOfClass : [SysWindow class]])
    pimpl->wnd = reinterpret_cast<SysWindow*>(w);

  const CGRect rect = windowRect(pimpl->wnd);
  sz = {int(rect.size.width), int(rect.size.height)};

  pimpl->view = [[MetalView alloc] initWithFrame:rect];
#if defined(__OSX__)
  pimpl->view.wantsLayer = YES;
  pimpl->wnd.contentView = pimpl->view;
#elif defined(__IOS__)
  pimpl->wnd.rootViewController.view = pimpl->view;
#endif

  CAMetalLayer* lay = pimpl->metalLayer();
  const float dpi = backingScaleFactor(pimpl->wnd);
    
  lay.device = id<MTLDevice>(dev.impl.get());
    
  [lay setContentsScale:dpi];
  //lay.maximumDrawableCount      = 2;
  lay.pixelFormat               = MTLPixelFormatBGRA8Unorm;
  lay.allowsNextDrawableTimeout = NO;
  lay.framebufferOnly           = NO;

  reset();
  }

MtSwapchain::~MtSwapchain() {
  if(pimpl->view!=nil)
    [pimpl->view release];
  }

void MtSwapchain::reset() {
  dev.waitIdle(); // pending commands

  std::lock_guard<SpinLock> guard(sync);

  // https://developer.apple.com/documentation/quartzcore/cametallayer?language=objc
  CAMetalLayer* lay = pimpl->metalLayer();
  auto wrect = windowRect(pimpl->wnd);
  // auto lrect = lay.frame;
  lay.drawableSize = wrect.size;
  sz       = {int(wrect.size.width), int(wrect.size.height)};
  imgCount = uint32_t(lay.maximumDrawableCount);

  @autoreleasepool {
    img.resize(imgCount);
    for(size_t i=0; i<imgCount; ++i)
      img[i].tex = mkTexture();
    }

  currentImg = 0;
  }

uint32_t MtSwapchain::currentBackBufferIndex() {
  return currentImg;
  }

void MtSwapchain::present() {
  auto pool = NsPtr<NS::AutoreleasePool>::init();
  
  CA::MetalLayer* lay      = reinterpret_cast<CA::MetalLayer*>(pimpl->metalLayer());
  uint32_t        i        = currentImg;
  auto            drawable = lay->nextDrawable();
  
  std::lock_guard<SpinLock> guard(sync);
  auto cmd = dev.queue->commandBuffer();
  auto enc = cmd->blitCommandEncoder();
  auto dr  = drawable->texture();

  if(dr->width()!=img[i].tex->width() || dr->height()!=img[i].tex->height()) {
    enc->endEncoding();
    throw SwapchainSuboptimal();
    }
  enc->copyFromTexture(img[i].tex.get(), 0, 0,
                       dr, 0, 0,
                       1, 1);
  enc->endEncoding();
  cmd->presentDrawable(drawable);

  dev.onSubmit();
  cmd->addCompletedHandler(^(MTL::CommandBuffer* c){
    MTL::CommandBufferStatus s = c->status();
    if(s==MTL::CommandBufferStatusNotEnqueued ||
       s==MTL::CommandBufferStatusEnqueued ||
       s==MTL::CommandBufferStatusCommitted ||
       s==MTL::CommandBufferStatusScheduled)
      return;

    if(s!=MTL::CommandBufferStatusCompleted)
      Log::e("swapchain fatal error");

    dev.onFinish();
    });
  cmd->commit();

  nextDrawable();
  }

NsPtr<MTL::Texture> MtSwapchain::mkTexture() {
  auto desc = NsPtr<MTL::TextureDescriptor>::init();
  if(desc==nullptr)
    throw std::system_error(GraphicsErrc::OutOfVideoMemory);

  desc->setTextureType(MTL::TextureType2D);
  desc->setPixelFormat(MTL::PixelFormatBGRA8Unorm);
  desc->setWidth(sz.w);
  desc->setHeight(sz.h);
  desc->setMipmapLevelCount(1);
  desc->setCpuCacheMode(MTL::CPUCacheModeDefaultCache);
  desc->setStorageMode(MTL::StorageModePrivate);
  desc->setUsage(MTL::TextureUsageRenderTarget);
  desc->setAllowGPUOptimizedContents(true);

  auto impl = NsPtr<MTL::Texture>(dev.impl->newTexture(desc.get()));
  if(impl==nullptr)
    throw std::system_error(GraphicsErrc::OutOfVideoMemory);
  return impl;
  }

void MtSwapchain::nextDrawable() {
  currentImg = (currentImg+1) % img.size();
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

MTL::PixelFormat MtSwapchain::format() const {
  CAMetalLayer* lay = pimpl->metalLayer();
  return MTL::PixelFormat(lay.pixelFormat);
  }

#endif
