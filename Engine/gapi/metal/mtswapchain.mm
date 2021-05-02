#include "mtswapchain.h"

#include <Tempest/Except>
#include <Tempest/Log>

#include "mtdevice.h"

#import <QuartzCore/CAMetalLayer.h>

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
MtSwapchain::MtSwapchain(MtDevice& dev, NSWindow *w):wnd(w) {
  NSRect rect = [wnd frame];
  sz = {int(rect.size.width), int(rect.size.height)};

  view = [[MetalView alloc] initWithFrame:rect];
  view.wantsLayer = YES;
  wnd.contentView = view;

  CAMetalLayer* lay = reinterpret_cast<CAMetalLayer*>(wnd.contentView.layer);
  lay.device = dev.impl;

  reset();
  }

MtSwapchain::~MtSwapchain() {
  releaseImg();
  if(view!=nil)
    [view release];
  }

void MtSwapchain::reset() {
  // https://developer.apple.com/documentation/quartzcore/cametallayer?language=objc
  releaseImg();

  CAMetalLayer* lay = reinterpret_cast<CAMetalLayer*>(wnd.contentView.layer);

  NSRect rect = [wnd frame];
  if(rect.size.width!=sz.w || rect.size.height!=sz.h) {
    lay.drawableSize = rect.size;
    sz = {int(rect.size.width), int(rect.size.height)};
    }

  imgCount = lay.maximumDrawableCount;

  std::vector<id<CAMetalDrawable>> vec(imgCount);
  img.reset(new id<MTLTexture>[imgCount]);
  for(size_t i=0; i<imgCount; ++i) {
    id<CAMetalDrawable> dr = [lay nextDrawable];
    vec[i] = dr;
    img[i] = dr.texture;
    }
  for(size_t i=0; i<vec.size(); ++i)
    [vec[i] release];
  }

uint32_t MtSwapchain::nextImage(AbstractGraphicsApi::Semaphore*) {
  // Log::d(__func__);
  releaseImg();

  CAMetalLayer* lay = reinterpret_cast<CAMetalLayer*>(wnd.contentView.layer);
  current = [lay nextDrawable];
  // HACK: assume that metal reusing same textures over and over unitil window-resize
  for(size_t i=0; i<imgCount; ++i)
    if(img[i]==current.texture)
      return i;
  Log::d("failed to recycle CAMetalLayer textures - force reset");
  throw SwapchainSuboptimal();
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

void MtSwapchain::releaseImg() {
  if(current!=nil) {
    [current release];
    current = nil;
    }
  }
