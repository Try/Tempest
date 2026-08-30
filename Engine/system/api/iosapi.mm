#include "iosapi.h"

#include <Tempest/Platform>
#include <Tempest/IOSRuntime>
#include <Tempest/Log>

#ifdef __IOS__

#import  <UIKit/UIKit.h>
#include <string>
#include <thread>
#include <TargetConditionals.h>

#include <Tempest/Window>
#include <Tempest/Event>

using namespace Tempest;

#if TARGET_CPU_X86_64
#  define FUNCTION_CALL_ALIGNMENT 16
#  define SET_STACK_POINTER "movq %0, %%rsp"
#elif TARGET_CPU_X86
#  define FUNCTION_CALL_ALIGNMENT 16
#  define SET_STACK_POINTER "mov %0, %%esp"
#elif TARGET_CPU_ARM || TARGET_CPU_ARM64
#  // Valid for both 32 and 64-bit ARM
#  define FUNCTION_CALL_ALIGNMENT 4
#  define SET_STACK_POINTER "mov sp, %0"
#else
#  error "Unknown processor family"
#endif

static uintptr_t alignDown(uintptr_t val, uintptr_t align) {
  return val & ~(align - 1);
  }

static void swapContext();

static void drawFrame();
static void resumeEngineFromUIKit();

@class TempestWindow;

static TempestWindow*  mainWindow             = nil;
static std::atomic_bool isRunning{true};
static std::atomic_bool isEngineReady{false};
static std::atomic_bool isApplicationActive{false};
static uint64_t         lifecycleGeneration   = 0;
static bool             activationResumePending = false;
static bool             idleTimerDisabled     = false;

enum class FrameRateMode : uint8_t {
  SystemDefault,
  Fixed,
  Range,
  };

static FrameRateMode frameRateMode      = FrameRateMode::SystemDefault;
static uint32_t      frameRateMinimum   = 0;
static uint32_t      frameRateMaximum   = 0;
static uint32_t      frameRatePreferred = 0;

@interface TempestWindow : UIWindow {
  @public Tempest::Window* owner;
  @public CADisplayLink*   displayLink;
  @public std::atomic_bool hasPendingFrame;
  
  struct Touch {
    const UITouch* id;
    CGPoint        pos;
    };
  
  union Ev {
    Ev(){}
    ~Ev(){}

    Event          noEvent;
    SizeEvent      size;
    MouseEvent     mouse;
    KeyEvent       key;
    CloseEvent     close;
    Tempest::Point move;
    } event;
  Event::Type curentEvent;
  
  struct TouchState {
    std::vector<Touch> touch;
    TouchState() {
      touch.reserve(2);
      }
    
    int add(const UITouch* id, const CGPoint& pos) {
      for(auto& t:touch)
        if(t.id==id)
          return -1;
      
      Touch tx = {};
      tx.id  = id;
      tx.pos = pos;
      
      for(size_t i=0; i<touch.size(); ++i)
        if(touch[i].id==nullptr) {
          touch[i] = tx;
          return int(i);
          }
      
      touch.push_back(tx);
      return int(touch.size()-1);
      }
    
    int del(const UITouch* id) {
      int res = -1;
      for(size_t i=0; i<touch.size(); ++i)
        if(touch[i].id==id) {
          touch[i].id = nullptr;
          if(res<0)
            res = int(i);
          }
      
      while(touch.size() && touch.back().id==nullptr)
        touch.pop_back();
      return res;
      }
    
    int update(const UITouch* id, const CGPoint& pos) {
      for(size_t i=0; i<touch.size(); ++i)
        if(touch[i].id==id){
          if(touch[i].pos.x==pos.x && touch[i].pos.y==pos.y)
            return -1;
          touch[i].pos = pos;
          return int(i);
          }
      return -1;
      }
    };
  TouchState touch;
  }
@end
@implementation TempestWindow
- (void)layoutSubviews {
  [super layoutSubviews];
  
  const CGFloat scale = self.contentScaleFactor;
  CGRect frame        = self.bounds;
  frame.origin.x      = 0;
  frame.origin.y      = 0;
  [self.rootViewController.view setFrame: frame];

  if(owner==nullptr || !isEngineReady.load() || !isApplicationActive.load())
    return;
  
  new (&event.size) SizeEvent(int32_t(frame.size.width*scale), int32_t(frame.size.height*scale));
  curentEvent = Event::Resize;
  activationResumePending = false;
  resumeEngineFromUIKit();
  }

- (void)drawFrame {
  hasPendingFrame.store(true);
  if(!isEngineReady.load() || !isApplicationActive.load())
    return;
  activationResumePending = false;
  resumeEngineFromUIKit();
  // drawFrame();
  }

- (void)touchesBegan:(NSSet *)touches withEvent:(UIEvent *)ex {
  const CGFloat scale = self.contentScaleFactor;
  for(UITouch *tx in touches) {
    CGPoint p  = [tx locationInView:self];
    int     id = touch.add(tx, p);
    if(id<0)
      continue;
    new (&event.mouse) MouseEvent(int(p.x*scale),
                                  int(p.y*scale),
                                  Event::ButtonLeft,
                                  Event::M_NoModifier,
                                  0,
                                  id,
                                  Event::MouseDown
                                  );
    curentEvent = Event::MouseDown;
    resumeEngineFromUIKit();
    }
  }

- (void)touchesMoved:(NSSet *)touches withEvent:(UIEvent *)ex {
  const CGFloat scale = self.contentScaleFactor;
  for(UITouch *tx in touches) {
    CGPoint p  = [tx locationInView:self];
    int     id = touch.update(tx,p);
    if(id<0)
      continue;
    new (&event.mouse) MouseEvent(int(p.x*scale),
                                  int(p.y*scale),
                                  Event::ButtonLeft,
                                  Event::M_NoModifier,
                                  0,
                                  id,
                                  Event::MouseMove
                                  );
    curentEvent = Event::MouseMove;
    resumeEngineFromUIKit();
    }
  }

- (void)touchesEnded:(NSSet *)touches withEvent:(UIEvent *)ex {
  const CGFloat scale = self.contentScaleFactor;
  for(UITouch *tx in touches) {
    CGPoint p  = [tx locationInView:self];
    int     id = touch.del(tx);
    if(id<0)
      continue;
    
    new (&event.mouse) MouseEvent(int(p.x*scale),
                                  int(p.y*scale),
                                  Event::ButtonLeft,
                                  Event::M_NoModifier,
                                  0,
                                  id,
                                  Event::MouseUp
                                  );
    curentEvent = Event::MouseUp;
    resumeEngineFromUIKit();
    }
  }

- (void)touchesCancelled:(NSSet *)touches withEvent:(UIEvent *)ex {
  [self touchesEnded:touches withEvent:ex];
  }
@end


@interface ViewController:UIViewController{}
-(id)init;
@end

@implementation ViewController {
  bool fullScreen;
  }

-(id)init {
  self = [super init];
  if(self!=nil)
    fullScreen = true;
  return self;
  }

- (void)viewDidLoad {
  self.extendedLayoutIncludesOpaqueBars = YES;
  //self.modalPresentationStyle = UIModalPresentationFullScreen;
  //[self setNeedsStatusBarAppearanceUpdate];
  //self.navigationController.isNavigationBarHidden = YES;
  //[self.navigationController setNavigationBarHidden: YES animated:YES];
  }

- (BOOL)prefersStatusBarHidden {
  return self->fullScreen ? YES : NO;
  }

- (BOOL) shouldAutorotate {
  return YES;
  }

- (BOOL)shouldAutorotateToInterfaceOrientation:(UIInterfaceOrientation)interfaceOrientation {
  (void)interfaceOrientation;
  return YES;
  }

-(UIInterfaceOrientationMask)supportedInterfaceOrientations {
  return UIInterfaceOrientationMaskAll;
  }

-(bool)setAsFullscreen: (bool)fullScreen {
  self->fullScreen = fullScreen;
  [self setNeedsStatusBarAppearanceUpdate];
  return true;
  }

-(bool)isFullscreen {
  return [self prefersStatusBarHidden];
  }
@end

static void applyPreferredFrameRate(CADisplayLink* displayLink) {
  if(displayLink==nil)
    return;
  if(@available(iOS 15.0, *)) {
    uint32_t screenMaximum = 60;
    if(mainWindow!=nil && mainWindow.screen!=nil &&
       mainWindow.screen.maximumFramesPerSecond>0)
      screenMaximum = uint32_t(mainWindow.screen.maximumFramesPerSecond);
    switch(frameRateMode) {
      case FrameRateMode::SystemDefault:
        displayLink.preferredFrameRateRange = CAFrameRateRangeDefault;
        break;
      case FrameRateMode::Fixed:
        {
        const uint32_t rate = frameRatePreferred<screenMaximum ?
                              frameRatePreferred : screenMaximum;
        displayLink.preferredFrameRateRange =
            CAFrameRateRangeMake(rate,rate,rate);
        break;
        }
      case FrameRateMode::Range:
        {
        const uint32_t maximum = frameRateMaximum<screenMaximum ?
                                 frameRateMaximum : screenMaximum;
        const uint32_t minimum = frameRateMinimum<maximum ?
                                 frameRateMinimum : maximum;
        const uint32_t preferred = frameRatePreferred<maximum ?
                                   (frameRatePreferred>minimum ?
                                    frameRatePreferred : minimum) : maximum;
        displayLink.preferredFrameRateRange =
            CAFrameRateRangeMake(minimum,maximum,preferred);
        break;
        }
      }
    }
  else {
    displayLink.preferredFramesPerSecond =
        frameRateMode==FrameRateMode::SystemDefault ? 0 :
        NSInteger(frameRatePreferred);
    }
  }

static void applyIdleTimerPreference() {
  [UIApplication sharedApplication].idleTimerDisabled =
      isApplicationActive.load() && idleTimerDisabled ? YES : NO;
  }

static void invalidateDisplayLink(TempestWindow* window) {
  if(window==nil)
    return;
  window->hasPendingFrame.store(false);
  [window->displayLink invalidate];
  window->displayLink = nil;
  isEngineReady.store(false);
  }

static void createDisplayLink(TempestWindow* window) {
  if(window==nil || window.windowScene==nil || window->owner==nullptr)
    return;
  if(window->displayLink==nil) {
    window->displayLink = [CADisplayLink displayLinkWithTarget:window
                                                     selector:@selector(drawFrame)];
    applyPreferredFrameRate(window->displayLink);
    [window->displayLink addToRunLoop:[NSRunLoop currentRunLoop]
                              forMode:NSRunLoopCommonModes];
    }
  window->displayLink.paused = !isApplicationActive.load();
  window->hasPendingFrame.store(true);
  isEngineReady.store(true);
  }

static TempestWindow* attachWindowToScene(UIWindowScene* windowScene) {
  if(mainWindow==nil) {
    mainWindow = [[TempestWindow alloc] initWithWindowScene:windowScene];
    ViewController* controller = [[ViewController alloc] init];
    mainWindow.rootViewController = controller;
    [controller release];
    mainWindow.autoresizingMask = UIViewAutoresizingFlexibleWidth |
                                  UIViewAutoresizingFlexibleHeight;
    mainWindow.backgroundColor = [UIColor blackColor];
    mainWindow->owner = nullptr;
    mainWindow->displayLink = nil;
    mainWindow->hasPendingFrame.store(false);
    mainWindow->curentEvent = Event::Type::NoEvent;
    }
  else {
    mainWindow.windowScene = windowScene;
    }

  mainWindow.frame = windowScene.coordinateSpace.bounds;
  mainWindow.contentScaleFactor = windowScene.screen.scale;
  createDisplayLink(mainWindow);
  return mainWindow;
  }

static void detachWindowFromScene(TempestWindow* window) {
  if(window==nil)
    return;
  invalidateDisplayLink(window);
  window.hidden = YES;
  window.windowScene = nil;
  }

@interface SceneDelegate : UIResponder <UIWindowSceneDelegate> {
  TempestWindow* window;
  uint64_t       activationGeneration;
  bool           connected;
  }
@property(nonatomic, retain) TempestWindow* window;
@end

@implementation SceneDelegate
@synthesize window;

- (void)scene:(UIScene *)scene
    willConnectToSession:(UISceneSession *)session
    options:(UISceneConnectionOptions *)connectionOptions {
  (void)session;
  (void)connectionOptions;
  if(![scene isKindOfClass:[UIWindowScene class]])
    return;

  [NSObject cancelPreviousPerformRequestsWithTarget:self];
  activationResumePending = false;
  isApplicationActive.store(false);
  connected = true;
  activationGeneration = ++lifecycleGeneration;
  self.window = attachWindowToScene((UIWindowScene*)scene);
  [self.window makeKeyAndVisible];
  }

- (void)sceneDidBecomeActive:(UIScene *)scene {
  if(!connected || scene!=self.window.windowScene)
    return;
  [NSObject cancelPreviousPerformRequestsWithTarget:self];
  isApplicationActive.store(true);
  applyIdleTimerPreference();
  if(self.window->displayLink!=nil)
    self.window->displayLink.paused = NO;
  activationResumePending = true;
  activationGeneration = ++lifecycleGeneration;
  [self performSelector:@selector(resumeEngineIfCurrent:)
             withObject:[NSNumber numberWithUnsignedLongLong:activationGeneration]
             afterDelay:0.1];
  }

- (void)sceneWillResignActive:(UIScene *)scene {
  if(!connected || scene!=self.window.windowScene)
    return;
  [NSObject cancelPreviousPerformRequestsWithTarget:self];
  activationGeneration = ++lifecycleGeneration;
  activationResumePending = false;
  isApplicationActive.store(false);
  applyIdleTimerPreference();
  self.window->hasPendingFrame.store(false);
  if(self.window->displayLink!=nil)
    self.window->displayLink.paused = YES;
  }

- (void)sceneDidDisconnect:(UIScene *)scene {
  if(!connected || scene!=self.window.windowScene)
    return;
  [NSObject cancelPreviousPerformRequestsWithTarget:self];
  activationGeneration = ++lifecycleGeneration;
  activationResumePending = false;
  isApplicationActive.store(false);
  applyIdleTimerPreference();
  detachWindowFromScene(self.window);
  connected = false;
  self.window = nil;
  }

- (void)resumeEngineIfCurrent:(NSNumber*)generation {
  if(!connected || !activationResumePending || !isApplicationActive.load() ||
     generation.unsignedLongLongValue!=activationGeneration ||
     activationGeneration!=lifecycleGeneration || self.window!=mainWindow)
    return;
  activationResumePending = false;
  resumeEngineFromUIKit();
  }

- (void)dealloc {
  [NSObject cancelPreviousPerformRequestsWithTarget:self];
  if(connected && self.window==mainWindow) {
    activationGeneration = ++lifecycleGeneration;
    activationResumePending = false;
    isApplicationActive.store(false);
    applyIdleTimerPreference();
    detachWindowFromScene(self.window);
    connected = false;
    }
  self.window = nil;
  [super dealloc];
  }
@end

@interface AppDelegate : NSObject <UIApplicationDelegate> {
  }
@end

@implementation AppDelegate
- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)launchOptions {
  (void)application;
  (void)launchOptions;
  return YES;
  }

- (UISceneConfiguration *)application:(UIApplication *)application
    configurationForConnectingSceneSession:(UISceneSession *)connectingSceneSession
    options:(UISceneConnectionOptions *)options {
  (void)application;
  (void)options;
  UISceneConfiguration* configuration =
      [UISceneConfiguration configurationWithName:@"Tempest Scene"
                                      sessionRole:connectingSceneSession.role];
  configuration.delegateClass = [SceneDelegate class];
  return configuration;
  }

- (UIInterfaceOrientationMask)application:(UIApplication *)application
  supportedInterfaceOrientationsForWindow:(UIWindow *)window {
  (void)application;
  (void)window;
  return UIInterfaceOrientationMaskAll;
  }

- (void)applicationWillTerminate:(UIApplication *)application {
  (void)application;
  ++lifecycleGeneration;
  activationResumePending = false;
  isApplicationActive.store(false);
  isRunning.store(false);
  idleTimerDisabled = false;
  applyIdleTimerPreference();
  frameRateMode      = FrameRateMode::SystemDefault;
  frameRateMinimum   = 0;
  frameRateMaximum   = 0;
  frameRatePreferred = 0;
  if(mainWindow!=nil) {
    invalidateDisplayLink(mainWindow);
    mainWindow->owner = nullptr;
    mainWindow.windowScene = nil;
    [mainWindow release];
    mainWindow = nil;
    }
  }
@end


struct Fiber  {
  jmp_buf jmp = {};
  };

static Fiber            mainContext;
static Fiber            appleContext;
static Fiber*           currentContext = nullptr;
// The engine and its script VM share this manually-swapped stack on iOS.
alignas(16) static char appleStack[8*1024*1024]={};
static             void appleMain(void*);

inline static void createAppleSubContext()  {
  if(_setjmp(mainContext.jmp) == 0) {
    // replace stack
    // static const long kPageSize = sysconf(_SC_PAGESIZE);

    __volatile__ uintptr_t ptr  = reinterpret_cast<uintptr_t>(appleStack);
    __volatile__ uintptr_t base = alignDown(ptr + sizeof(appleStack), FUNCTION_CALL_ALIGNMENT);

    __asm__ __volatile__(
                SET_STACK_POINTER
                : // no outputs
                : "r" (alignDown(base, FUNCTION_CALL_ALIGNMENT))
            );

    std::atomic_thread_fence(std::memory_order_seq_cst);
    currentContext = &appleContext;
    appleMain(nullptr);
    }
  }

inline static void swapContext() {
  std::atomic_thread_fence(std::memory_order_seq_cst);
  if(currentContext==&appleContext) {
    currentContext = &mainContext;
    if(_setjmp(appleContext.jmp) == 0)
      _longjmp(mainContext.jmp, 1);
    } else {
    currentContext = &appleContext;
    if(_setjmp(mainContext.jmp) == 0)
      _longjmp(appleContext.jmp, 1);
    }
  std::atomic_thread_fence(std::memory_order_seq_cst);
  }

static void resumeEngineFromUIKit() {
  if(currentContext==&appleContext)
    swapContext();
  }

void Tempest::iOS::yieldToUIKit() {
  if(![NSThread isMainThread] || currentContext!=&mainContext ||
     !isApplicationActive.load() || mainWindow==nil ||
     mainWindow->displayLink==nil)
    return;
  swapContext();
  }

void Tempest::iOS::setPreferredFrameRate(uint32_t framesPerSecond) {
  if(![NSThread isMainThread])
    return;
  frameRateMode      = framesPerSecond==0 ? FrameRateMode::SystemDefault :
                                               FrameRateMode::Fixed;
  frameRateMinimum   = framesPerSecond;
  frameRateMaximum   = framesPerSecond;
  frameRatePreferred = framesPerSecond;
  if(mainWindow!=nil)
    applyPreferredFrameRate(mainWindow->displayLink);
  }

void Tempest::iOS::setPreferredFrameRateRange(uint32_t minimumFramesPerSecond,
                                              uint32_t maximumFramesPerSecond,
                                              uint32_t preferredFramesPerSecond) {
  if(![NSThread isMainThread])
    return;
  if(maximumFramesPerSecond==0) {
    setPreferredFrameRate(0);
    return;
    }
  if(minimumFramesPerSecond==0)
    minimumFramesPerSecond = 1;
  if(maximumFramesPerSecond<minimumFramesPerSecond)
    maximumFramesPerSecond = minimumFramesPerSecond;
  if(preferredFramesPerSecond<minimumFramesPerSecond)
    preferredFramesPerSecond = minimumFramesPerSecond;
  if(preferredFramesPerSecond>maximumFramesPerSecond)
    preferredFramesPerSecond = maximumFramesPerSecond;

  frameRateMode      = FrameRateMode::Range;
  frameRateMinimum   = minimumFramesPerSecond;
  frameRateMaximum   = maximumFramesPerSecond;
  frameRatePreferred = preferredFramesPerSecond;
  if(mainWindow!=nil)
    applyPreferredFrameRate(mainWindow->displayLink);
  }

void Tempest::iOS::setIdleTimerDisabled(bool disabled) {
  if(![NSThread isMainThread])
    return;
  idleTimerDisabled = disabled;
  applyIdleTimerPreference();
  }

static void drawFrame() {
  auto cb = (mainWindow->owner);
  @autoreleasepool {
    if(cb!=nullptr) {
      mainWindow->hasPendingFrame.store(false);
      iOSApi::dispatchRender(*cb);
      }
    }
  }

static void appleMain(void*) {
  static std::string app = "application";
  char * argv[2] = {
    &app[0], nullptr
    };
  UIApplicationMain(1, argv, nil, NSStringFromClass( [ AppDelegate class ] ) );
  }

static SystemApi::Window* createWindow(Tempest::Window *owner, uint32_t w, uint32_t h, SystemApi::ShowMode mode) {
  (void)w;
  (void)h;
  (void)mode;
  auto window = mainWindow;
  if(window==nil)
    return nullptr;

  window->owner = owner;
  createDisplayLink(window);
  
  return reinterpret_cast<SystemApi::Window*>(window);
  }

iOSApi::iOSApi() {
  createAppleSubContext();
  }

SystemApi::Window *iOSApi::implCreateWindow(Tempest::Window *owner, uint32_t width, uint32_t height) {
  return ::createWindow(owner, width, height, ShowMode::Maximized);
  }

SystemApi::Window *iOSApi::implCreateWindow(Tempest::Window *owner, SystemApi::ShowMode sm) {
  return ::createWindow(owner, 800, 600, sm);
  }

void iOSApi::implDestroyWindow(SystemApi::Window *w) {
  auto wx = reinterpret_cast<TempestWindow*>(w);
  if(wx==nullptr)
    return;
  wx->owner = nullptr;
  invalidateDisplayLink(wx);
  }

void iOSApi::implExit() {
  ::isRunning.store(false);
  activationResumePending = false;
  isEngineReady.store(false);
  invalidateDisplayLink(mainWindow);
  }

Tempest::Rect iOSApi::implWindowClientRect(Window* w) {
  auto wx = reinterpret_cast<TempestWindow*>(w);
  const CGFloat scale = wx.contentScaleFactor;
  const CGRect  frame = wx.frame;
  //wner->resize(int32_t(frame.size.width*scale), int32_t(frame.size.height*scale));
  
  //CGRect rect = [ [ UIScreen mainScreen ] bounds ];
  return Rect(int32_t(frame.origin.x*scale), int32_t(frame.origin.y*scale),
              int32_t(frame.size.width*scale), int32_t(frame.size.height*scale));
  }

bool iOSApi::implSetAsFullscreen(Window* w, bool fullScreen) {
  auto wx = reinterpret_cast<TempestWindow*>(w);
  ViewController* ctrl = reinterpret_cast<ViewController*>(wx.rootViewController);
  return [ctrl setAsFullscreen: fullScreen];
  }

bool iOSApi::implIsFullscreen(Window* w) {
  auto wx = reinterpret_cast<TempestWindow*>(w);
  ViewController* ctrl = reinterpret_cast<ViewController*>(wx.rootViewController);
  return [ctrl isFullscreen];
  }

void iOSApi::implSetCursorPosition(Window* w, int x, int y) {
  }

void iOSApi::implShowCursor(Window* w, CursorShape cursor) {
  }

bool iOSApi::implIsRunning() {
  return ::isRunning.load();
  }

int iOSApi::implExec(AppCallBack& cb) {
  while(::isRunning.load()) {
    implProcessEvents(cb);
    if(!cb.onTimer())
      std::this_thread::yield();
    }
  return 0;
  }

void iOSApi::implProcessEvents(AppCallBack& cb) {
  if(mainWindow==nil || mainWindow->owner==nullptr) {
    swapContext();
    return;
    }
  
  // The engine and UIKit fibers share one OS thread. An Objective-C pool
  // pushed on the engine fiber can be invalidated while UIKit runs and then
  // trigger AutoreleasePoolPage::badPop when this stack resumes.
  {
    auto& wnd   = *mainWindow->owner;
    auto  eType = mainWindow->curentEvent;
    mainWindow->curentEvent = Event::Type::NoEvent;
    
    switch(eType) {
      case Event::Type::Resize: {
        auto evt = mainWindow->event.size;
        mainWindow->event.size.~SizeEvent();
        iOSApi::dispatchResize(wnd, evt);
        break;
        }
      case Event::MouseDown:
      case Event::MouseMove:
      case Event::MouseUp: {
        auto evt = mainWindow->event.mouse;
        mainWindow->event.mouse.~MouseEvent();
        if(eType==Event::MouseDown)
          iOSApi::dispatchMouseDown(wnd, evt);
        else if(eType==Event::MouseMove)
          iOSApi::dispatchMouseMove(wnd, evt);
        else if(eType==Event::MouseUp)
          iOSApi::dispatchMouseUp(wnd, evt);
        break;
        }
      default:
        if(isApplicationActive.load() && mainWindow->hasPendingFrame.load()) {
          mainWindow->hasPendingFrame.store(false);
          iOSApi::dispatchRender(wnd);
          }
        break;
      }
    }
  swapContext();
  }

void iOSApi::implSetWindowTitle(Window* w, const char* utf8) {

  }

#endif
