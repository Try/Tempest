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
  return w.contentScaleFactor;
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
  : dev(dev), pimpl(new Impl()), currentAtt(dev, *this) {
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
#if defined(__IOS__)
  // Swapchain takes too much memory on 2GB iPhone
  lay.maximumDrawableCount      = 2;
#endif
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

  // https://developer.apple.com/documentation/quartzcore/cametallayer?language=objc
  CAMetalLayer* lay = pimpl->metalLayer();
  auto wrect = windowRect(pimpl->wnd);
  // auto lrect = lay.frame;
  lay.drawableSize = wrect.size;
  sz       = {int(wrect.size.width), int(wrect.size.height)};
  imgCount = uint32_t(lay.maximumDrawableCount);
  }

AbstractGraphicsApi::Texture* MtSwapchain::currentBackBuffer() {
  auto pool = NsPtr<NS::AutoreleasePool>::init();

  /*
  // possible to soft-detect window resize in theory
  auto wrect = windowRect(pimpl->wnd);
  if(int(wrect.size.width)!=sz.w || int(wrect.size.height)!=sz.h) {
    throw SwapchainSuboptimal();
    }
  */

  CA::MetalLayer* lay      = reinterpret_cast<CA::MetalLayer*>(pimpl->metalLayer());
  auto            drawable = lay->nextDrawable();
  if(drawable==nullptr) {
    throw SwapchainSuboptimal();
    }

  auto dr = drawable->texture();
  if(dr->width()!=sz.w || dr->height()!=sz.h) {
    throw SwapchainSuboptimal();
    }

  currentDrawable   = NsPtr<CA::MetalDrawable>(drawable->retain());
  currentAtt.swImpl = dr;
  currentAtt.format = MTL::PixelFormat(lay->pixelFormat());
  return &currentAtt;
  }

void MtSwapchain::present() {
  auto pool = NsPtr<NS::AutoreleasePool>::init();

  auto desc = NsPtr<MTL::CommandBufferDescriptor>::init();
  //desc->setRetainedReferences(true);
  desc->setErrorOptions(MTL::CommandBufferErrorOptionEncoderExecutionStatus);

  // To avoid presenting a drawable before any work is scheduled ...
  // call a command buffer's presentDrawable: method instead.
  // https://developer.apple.com/documentation/metal/mtldrawable/present%28%29?language=objc
  auto cmd = dev.queue->commandBuffer(desc.get());
  cmd->presentDrawable(currentDrawable.get());
  dev.onSubmit();
  cmd->addCompletedHandler(^(MTL::CommandBuffer* c){
      MTL::CommandBufferStatus s = c->status();
      if(s==MTL::CommandBufferStatusNotEnqueued ||
          s==MTL::CommandBufferStatusEnqueued ||
          s==MTL::CommandBufferStatusCommitted ||
          s==MTL::CommandBufferStatusScheduled)
        return;

      if(s!=MTL::CommandBufferStatusCompleted) {
        Log::e("swapchain fatal error");
        dev.onFinish();
        dev.waitIdle();
        return;
      }
      dev.onFinish();
    });
  cmd->commit();

  currentDrawable   = nullptr;
  currentAtt.swImpl = nullptr;
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
