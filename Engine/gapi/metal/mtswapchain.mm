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

#include <atomic>
#include <utility>

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

template<class T>
static NsPtr<T> strongRef(T* ptr) {
  if(ptr!=nullptr)
    ptr->retain();
  return NsPtr<T>(ptr);
  }

MtSwapchainFrame::MtSwapchainFrame(uint64_t generation, uint32_t image,
                                   bool direct, MTL::Texture* texture,
                                   CA::MetalDrawable* drawable)
  :generation(generation), image(image), direct(direct),
   texture(strongRef(texture)), drawable(strongRef(drawable)) {
  }

MtSwapchainFrame::~MtSwapchainFrame() {
  }

namespace {

struct PresentFrame final {
  MtSwapchain::Frame      rendered;
  NsPtr<CA::MetalDrawable> drawable;

  PresentFrame(MtSwapchain::Frame rendered, CA::MetalDrawable* drawable)
    :rendered(std::move(rendered)), drawable(strongRef(drawable)) {
    }
  };

class DeviceSubmission final {
  public:
    explicit DeviceSubmission(MtDevice* device) :device(device) {
      device->onSubmit();
      }

    ~DeviceSubmission() {
      finish();
      }

    void finish() noexcept {
      if(!finished.exchange(true,std::memory_order_acq_rel))
        device->onFinish();
      }

  private:
    MtDevice*         device = nullptr;
    std::atomic_bool  finished{false};
  };

}

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
MtSwapchain::MtSwapchain(MtDevice& dev, SystemApi::Window *w,
                         uint32_t bufferCount, bool directPreferred)
  :pimpl(new Impl()), dev(dev), directPreferred(directPreferred) {
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
  lay.maximumDrawableCount      = bufferCount==0 ? 2 : bufferCount;
#elif defined(__OSX__)
  if(bufferCount!=0)
    lay.maximumDrawableCount = bufferCount;
#endif
  lay.pixelFormat               = MTLPixelFormatBGRA8Unorm;
  lay.allowsNextDrawableTimeout = directPreferred ? YES : NO;
  lay.framebufferOnly           = NO;

  reset();
  }

MtSwapchain::~MtSwapchain() {
  Frame              retiredFrame;
  std::vector<Image> retiredImages;
  auto exclusive = operationGate.blockNewOperations();
  {
    std::lock_guard<SpinLock> guard(sync);
    state.reset(0);
    activeFrame.swap(retiredFrame);
    img.swap(retiredImages);
    sz = {0,0};
    }
  exclusive.wait();
  dev.waitIdle();

  // Releasing Metal objects can enter the Objective-C runtime. Keep it out of
  // the swapchain spinlock, including during destruction.
  retiredFrame.reset();
  retiredImages.clear();

  if(pimpl->view!=nil)
    [pimpl->view release];
  }

void MtSwapchain::reset() {
  Frame              retiredFrame;
  std::vector<Image> retiredImages;
  // First stop new CPU operations, then invalidate all tickets. Active
  // acquire/present operations can unwind without contending on the gate.
  auto exclusive = operationGate.blockNewOperations();
  {
    std::lock_guard<SpinLock> guard(sync);
    state.reset(0);
    activeFrame.swap(retiredFrame);
    img.swap(retiredImages);
    sz = {0,0};
    }

  // No layer/device call is made under the swapchain spinlock.
  exclusive.wait();
  dev.waitIdle(); // pending commands
  retiredFrame.reset();
  retiredImages.clear();

  // https://developer.apple.com/documentation/quartzcore/cametallayer?language=objc
  CAMetalLayer* lay = pimpl->metalLayer();
  auto wrect = windowRect(pimpl->wnd);
  // auto lrect = lay.frame;
  lay.drawableSize = wrect.size;
  const Tempest::Size newSize = {int(wrect.size.width), int(wrect.size.height)};
  const uint32_t imageCount   = uint32_t(lay.maximumDrawableCount);

  std::vector<Image> newImages(imageCount);
  if(!directPreferred) {
    // Preserve the established Copy path: all private back buffers are
    // allocated eagerly during reset.
    for(auto& image:newImages)
      image.tex = mkTexture(newSize);
    }

  {
    std::lock_guard<SpinLock> guard(sync);
    sz  = newSize;
    img.swap(newImages);
    state.reset(imageCount);
    }
  }

uint32_t MtSwapchain::currentBackBufferIndex() {
  std::lock_guard<SpinLock> guard(sync);
  return state.currentImage();
  }

MtSwapchain::Frame MtSwapchain::acquireCopyFrame(const MtSwapchainState::Ticket& ticket) {
  Tempest::Size expected;
  MTL::Texture* existing = nullptr;
  bool valid = false;
  {
    std::lock_guard<SpinLock> guard(sync);
    valid = state.isAcquiring(ticket) && ticket.image<img.size();
    if(valid) {
      expected = sz;
      existing = img[ticket.image].tex.get();
      }
    }
  if(!valid)
    throw SwapchainSuboptimal();
  if(existing!=nullptr)
    return std::make_shared<MtSwapchainFrame>(ticket.generation,ticket.image,
                                              false,existing,nullptr);

  // Direct mode allocates its private fallback only when drawable acquisition
  // fails. Texture allocation is intentionally outside the state lock.
  auto created = mkTexture(expected);
  MTL::Texture* selected = nullptr;
  {
    std::lock_guard<SpinLock> guard(sync);
    valid = state.isAcquiring(ticket) && ticket.image<img.size() &&
            sz.w==expected.w && sz.h==expected.h;
    if(valid) {
      if(img[ticket.image].tex==nullptr)
        img[ticket.image].tex = std::move(created);
      selected = img[ticket.image].tex.get();
      }
    }
  if(!valid)
    throw SwapchainSuboptimal();
  return std::make_shared<MtSwapchainFrame>(ticket.generation,ticket.image,
                                            false,selected,nullptr);
  }

MtSwapchain::Frame MtSwapchain::acquireRenderTarget(uint32_t image) {
  auto operation = operationGate.startOperation();
  return acquireRenderTargetImpl(image);
  }

MtSwapchain::Frame MtSwapchain::acquireRenderTargetImpl(uint32_t image) {
  MtSwapchainState::Acquire acquire;
  Tempest::Size expected;
  Frame reuse;
  bool invalid = false;
  {
    std::lock_guard<SpinLock> guard(sync);
    acquire = state.beginAcquire();
    if(acquire.result==MtSwapchainState::Acquire::Result::Reuse) {
      invalid = activeFrame==nullptr || image!=acquire.ticket.image;
      if(!invalid)
        reuse = activeFrame;
      }
    else if(acquire.result!=MtSwapchainState::Acquire::Result::Start ||
            image!=acquire.ticket.image) {
      if(acquire.result==MtSwapchainState::Acquire::Result::Start)
        state.cancelAcquire(acquire.ticket);
      invalid = true;
      }
    else {
      expected = sz;
      }
    }
  if(invalid)
    throw SwapchainSuboptimal();
  if(reuse!=nullptr)
    return reuse;

  Frame frame;
  try {
    if(directPreferred) {
      auto pool = NsPtr<NS::AutoreleasePool>::init();
      auto* lay = reinterpret_cast<CA::MetalLayer*>(pimpl->metalLayer());
      auto drawable = strongRef(lay->nextDrawable());
      auto* texture  = drawable==nullptr ? nullptr : drawable->texture();
      const bool sizeMatches = texture!=nullptr &&
                               texture->width()==uint32_t(expected.w) &&
                               texture->height()==uint32_t(expected.h);
      if(MtSwapchainState::chooseTarget(true,texture!=nullptr,sizeMatches)==
         MtSwapchainState::Target::Direct) {
        frame = std::make_shared<MtSwapchainFrame>(acquire.ticket.generation,
                                                   acquire.ticket.image,true,
                                                   texture,drawable.get());
        }
      }

    if(frame==nullptr)
      frame = acquireCopyFrame(acquire.ticket);

    // Prepare the shared ownership before entering the spinlock. Swapping it
    // into activeFrame cannot allocate or release the previous frame there.
    Frame publishedFrame = frame;
    bool published = false;
    {
      std::lock_guard<SpinLock> guard(sync);
      published = state.publish(acquire.ticket,
                                frame->direct ? MtSwapchainState::Target::Direct
                                              : MtSwapchainState::Target::Copy);
      if(published)
        activeFrame.swap(publishedFrame);
      }
    if(!published)
      throw SwapchainSuboptimal();
    return frame;
    }
  catch(...) {
    {
      std::lock_guard<SpinLock> guard(sync);
      state.cancelAcquire(acquire.ticket);
      }
    throw;
    }
  }

void MtSwapchain::present() {
  auto operation = operationGate.startOperation();
  MtSwapchainState::Ticket ticket;
  Frame frame;
  bool acquireCopy = false;
  uint32_t copyImage = 0;
  bool invalid = false;
  {
    std::lock_guard<SpinLock> guard(sync);
    if(state.beginPresent(ticket)) {
      if(activeFrame==nullptr) {
        state.presentFailed(ticket);
        invalid = true;
        }
      else {
        frame = activeFrame;
        }
      }
    else if(!directPreferred && state.currentPhase()==MtSwapchainState::Phase::Idle) {
      // The old Copy implementation allowed presenting an untouched private
      // back buffer. Preserve that edge case for source/behaviour compatibility.
      acquireCopy = true;
      copyImage   = state.currentImage();
      }
    else {
      invalid = true;
      }
    }
  if(invalid)
    throw SwapchainSuboptimal();

  if(acquireCopy) {
    acquireRenderTargetImpl(copyImage);
    {
      std::lock_guard<SpinLock> guard(sync);
      const bool began = state.beginPresent(ticket);
      invalid = !began || activeFrame==nullptr;
      if(invalid) {
        if(began)
          state.presentFailed(ticket);
        }
      else {
        frame = activeFrame;
        }
      }
    if(invalid)
      throw SwapchainSuboptimal();
    }

  bool committed = false;
  std::shared_ptr<DeviceSubmission> submission;
  try {
    auto pool = NsPtr<NS::AutoreleasePool>::init();
    CA::MetalDrawable* drawable = frame->drawable.get();
    NsPtr<CA::MetalDrawable> acquired;
    if(drawable==nullptr) {
      auto* lay = reinterpret_cast<CA::MetalLayer*>(pimpl->metalLayer());
      acquired = strongRef(lay->nextDrawable());
      drawable = acquired.get();
      }

    auto* target = drawable==nullptr ? nullptr : drawable->texture();
    if(target==nullptr || target->width()!=frame->texture->width() ||
       target->height()!=frame->texture->height())
      throw SwapchainSuboptimal();

    auto keepAlive = std::make_shared<PresentFrame>(frame,drawable);
    auto desc      = NsPtr<MTL::CommandBufferDescriptor>::init();
    if(desc==nullptr)
      throw std::system_error(GraphicsErrc::OutOfVideoMemory);
    desc->setRetainedReferences(true);
    desc->setErrorOptions(MTL::CommandBufferErrorOptionEncoderExecutionStatus);

    auto* cmd = dev.queue->commandBuffer(desc.get());
    if(cmd==nullptr)
      throw std::system_error(GraphicsErrc::OutOfVideoMemory);

    if(!frame->direct) {
      auto* enc = cmd->blitCommandEncoder();
      if(enc==nullptr)
        throw std::system_error(GraphicsErrc::OutOfVideoMemory);
      enc->copyFromTexture(frame->texture.get(), 0, 0,
                           target, 0, 0,
                           1, 1);
      enc->endEncoding();
      }
    cmd->presentDrawable(drawable);
    auto* stableDevice = &dev;
    submission = std::make_shared<DeviceSubmission>(stableDevice);
    cmd->addCompletedHandler(^(MTL::CommandBuffer* c){
      (void)keepAlive;
      const MTL::CommandBufferStatus status = c->status();
      if(status!=MTL::CommandBufferStatusCompleted)
        Log::e("swapchain fatal error");
      submission->finish();
      });
    cmd->commit();
    committed = true;

    Frame retiredFrame;
    {
      std::lock_guard<SpinLock> guard(sync);
      if(state.presentCommitted(ticket))
        activeFrame.swap(retiredFrame);
      }
    }
  catch(...) {
    if(!committed) {
      if(submission!=nullptr)
        submission->finish();
      {
        std::lock_guard<SpinLock> guard(sync);
        state.presentFailed(ticket);
        }
      }
    throw;
    }
  }

NsPtr<MTL::Texture> MtSwapchain::mkTexture(const Tempest::Size& size) {
  auto pool = NsPtr<NS::AutoreleasePool>::init();
  auto desc = NsPtr<MTL::TextureDescriptor>::init();
  if(desc==nullptr)
    throw std::system_error(GraphicsErrc::OutOfVideoMemory);

  desc->setTextureType(MTL::TextureType2D);
  desc->setPixelFormat(MTL::PixelFormatBGRA8Unorm);
  desc->setWidth(size.w);
  desc->setHeight(size.h);
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

uint32_t MtSwapchain::imageCount() const {
  std::lock_guard<SpinLock> guard(sync);
  return state.imageCount();
  }

uint32_t MtSwapchain::w() const {
  std::lock_guard<SpinLock> guard(sync);
  return sz.w;
  }

uint32_t MtSwapchain::h() const {
  std::lock_guard<SpinLock> guard(sync);
  return sz.h;
  }

MTL::PixelFormat MtSwapchain::format() const {
  CAMetalLayer* lay = pimpl->metalLayer();
  return MTL::PixelFormat(lay.pixelFormat);
  }

#endif
