#include "iosapi.h"

#include <Tempest/Platform>
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
@class TempestSceneDelegate;

static TempestWindow*        mainWindow          = nil;
static TempestSceneDelegate* activeSceneDelegate = nil;
static UIWindowScene* activeWindowScene API_AVAILABLE(ios(13.0)) = nil;
static std::atomic_bool isRunning{true};
static std::atomic_bool isEngineReady{false};
static std::atomic_bool isApplicationActive{false};
static uint64_t         lifecycleGeneration      = 0;
static bool             activationResumePending = false;
static bool             usesSceneLifecycle      = false;

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

    void clear() {
      touch.clear();
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

- (void)drawFrame:(CADisplayLink*)sender {
  (void)sender;
  hasPendingFrame.store(true);
  if(owner==nullptr || !isEngineReady.load() || !isApplicationActive.load())
    return;
  activationResumePending = false;
  resumeEngineFromUIKit();
  // drawFrame();
  }

- (void)touchesBegan:(NSSet *)touches withEvent:(UIEvent *)ex {
  if(owner==nullptr)
    return;
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
    activationResumePending = false;
    resumeEngineFromUIKit();
    }
  }

- (void)touchesMoved:(NSSet *)touches withEvent:(UIEvent *)ex {
  if(owner==nullptr)
    return;
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
    activationResumePending = false;
    resumeEngineFromUIKit();
    }
  }

- (void)touchesEnded:(NSSet *)touches withEvent:(UIEvent *)ex {
  if(owner==nullptr)
    return;
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
    activationResumePending = false;
    resumeEngineFromUIKit();
    }
  }

- (void)touchesCancelled:(NSSet *)touches withEvent:(UIEvent *)ex {
  [self touchesEnded:touches withEvent:ex];
  }
@end

static void discardPendingEvent(TempestWindow* window) {
  switch(window->curentEvent) {
    case Event::Resize:
      window->event.size.~SizeEvent();
      break;
    case Event::MouseDown:
    case Event::MouseMove:
    case Event::MouseUp:
      window->event.mouse.~MouseEvent();
      break;
    default:
      break;
    }
  window->curentEvent = Event::NoEvent;
  }

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
  [super viewDidLoad];
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

-(bool)setAsFullscreen: (bool)value {
  self->fullScreen = value;
  [self setNeedsStatusBarAppearanceUpdate];
  return true;
  }

-(bool)isFullscreen {
  return [self prefersStatusBarHidden];
  }
@end

static void invalidateDisplayLink(TempestWindow* window) {
  if(window==nil)
    return;
  window->hasPendingFrame.store(false);
  [window->displayLink invalidate];
  window->displayLink = nil;
  isEngineReady.store(false);
  }

static void createDisplayLink(TempestWindow* window) {
  if(window==nil || window->owner==nullptr)
    return;
  if(window->displayLink==nil) {
    window->displayLink = [CADisplayLink displayLinkWithTarget:window
                                                     selector:@selector(drawFrame:)];
    [window->displayLink addToRunLoop:[NSRunLoop currentRunLoop]
                              forMode:NSRunLoopCommonModes];
    }
  window->displayLink.paused = !isApplicationActive.load();
  window->hasPendingFrame.store(true);
  isEngineReady.store(true);
  }

static bool initializeWindow(TempestWindow* window) {
  if(window==nil)
    return false;
  ViewController* controller = [[ViewController alloc] init];
  if(controller==nil)
    return false;
  [window setRootViewController:controller];
  [controller release];
  window.autoresizingMask = UIViewAutoresizingFlexibleWidth |
                            UIViewAutoresizingFlexibleHeight;
  window.backgroundColor = [UIColor blackColor];
  window->owner = nullptr;
  window->displayLink = nil;
  window->hasPendingFrame.store(false);
  window->curentEvent = Event::Type::NoEvent;
  return true;
  }

static bool attachWindowToScene(TempestWindow* window, UIWindowScene* windowScene)
    API_AVAILABLE(ios(13.0)) {
  if(window==nil || windowScene==nil)
    return false;
  if(window.windowScene==nil) {
    window.windowScene = windowScene;
    }
  else if(window.windowScene!=windowScene) {
    // Tempest exposes one native window and therefore accepts one iOS scene.
    return false;
    }

#if defined(__IPHONE_26_0) && \
    __IPHONE_OS_VERSION_MAX_ALLOWED >= __IPHONE_26_0
  if(@available(iOS 26.0, *)) {
    window.frame = windowScene.effectiveGeometry.coordinateSpace.bounds;
    }
  else
#endif
    window.frame = windowScene.coordinateSpace.bounds;
  window.contentScaleFactor = windowScene.screen.scale;
  return true;
  }

static void detachWindowFromScene(TempestWindow* window)
    API_AVAILABLE(ios(13.0)) {
  if(window==nil)
    return;
  invalidateDisplayLink(window);
  window.hidden = YES;
  window.windowScene = nil;
  }

static void scheduleEngineResume(NSObject* target, SEL selector,
                                 uint64_t generation) {
  [[NSRunLoop mainRunLoop]
      performSelector:selector
               target:target
             argument:[NSNumber numberWithUnsignedLongLong:generation]
                order:0
                modes:@[NSRunLoopCommonModes]];
  }

static void cancelEngineResume(NSObject* target) {
  [[NSRunLoop mainRunLoop] cancelPerformSelectorsWithTarget:target];
  }

static void activateWindow(NSObject* target, SEL resumeSelector,
                           uint64_t& generation, TempestWindow* window) {
  cancelEngineResume(target);
  isApplicationActive.store(true);
  createDisplayLink(window);
  activationResumePending = true;
  generation = ++lifecycleGeneration;
  scheduleEngineResume(target,resumeSelector,generation);
  }

static void deactivateWindow(NSObject* target, uint64_t& generation,
                             TempestWindow* window) {
  cancelEngineResume(target);
  generation = ++lifecycleGeneration;
  activationResumePending = false;
  isApplicationActive.store(false);
  if(window!=nil) {
    window->hasPendingFrame.store(false);
    if(window->displayLink!=nil)
      window->displayLink.paused = YES;
    }
  }

static void resumeEngineIfCurrent(NSNumber* scheduledGeneration,
                                  uint64_t generation,
                                  TempestWindow* window) {
  if(!activationResumePending || !isApplicationActive.load() ||
     scheduledGeneration.unsignedLongLongValue!=generation ||
     generation!=lifecycleGeneration || window!=mainWindow)
    return;
  activationResumePending = false;
  resumeEngineFromUIKit();
  }

API_AVAILABLE(ios(13.0))
@interface TempestSceneDelegate : UIResponder <UIWindowSceneDelegate> {
  TempestWindow* window;
  uint64_t       activationGeneration;
  bool           connected;
  }
@property(nonatomic, retain) TempestWindow* window;
@end

@implementation TempestSceneDelegate
@synthesize window;

- (void)scene:(UIScene *)scene
    willConnectToSession:(UISceneSession *)session
    options:(UISceneConnectionOptions *)connectionOptions {
  (void)connectionOptions;
  if(![scene isKindOfClass:[UIWindowScene class]])
    return;

  auto windowScene = (UIWindowScene*)scene;
  if(activeSceneDelegate!=nil && activeSceneDelegate!=self) {
    Log::e("Unable to attach a second iOS scene to the Tempest window");
    [[UIApplication sharedApplication] requestSceneSessionDestruction:session
                                                               options:nil
                                                          errorHandler:nil];
    return;
    }
  if(mainWindow!=nil && !attachWindowToScene(mainWindow,windowScene)) {
    Log::e("Unable to attach the Tempest window to the iOS scene");
    [[UIApplication sharedApplication] requestSceneSessionDestruction:session
                                                               options:nil
                                                          errorHandler:nil];
    return;
    }

  activeSceneDelegate = self;
  activeWindowScene = windowScene;
  connected = true;
  self.window = mainWindow;
  deactivateWindow(self,activationGeneration,mainWindow);
  if(self.window!=nil)
    [self.window makeKeyAndVisible];
  }

- (void)sceneDidBecomeActive:(UIScene *)scene {
  if(!connected || activeSceneDelegate!=self || scene!=activeWindowScene)
    return;
  activateWindow(self,@selector(resumeEngineIfCurrent:),
                 activationGeneration,mainWindow);
  }

- (void)sceneWillResignActive:(UIScene *)scene {
  if(!connected || activeSceneDelegate!=self || scene!=activeWindowScene)
    return;
  deactivateWindow(self,activationGeneration,mainWindow);
  }

- (void)sceneDidDisconnect:(UIScene *)scene {
  if(!connected || activeSceneDelegate!=self || scene!=activeWindowScene)
    return;
  deactivateWindow(self,activationGeneration,mainWindow);
  detachWindowFromScene(mainWindow);
  activeSceneDelegate = nil;
  activeWindowScene = nil;
  connected = false;
  self.window = nil;
  }

- (void)resumeEngineIfCurrent:(NSNumber*)generation {
  if(!connected || activeSceneDelegate!=self)
    return;
  ::resumeEngineIfCurrent(generation,activationGeneration,self.window);
  }

- (void)dealloc {
  if(connected && activeSceneDelegate==self) {
    deactivateWindow(self,activationGeneration,mainWindow);
    detachWindowFromScene(mainWindow);
    activeSceneDelegate = nil;
    activeWindowScene = nil;
    connected = false;
    }
  else {
    cancelEngineResume(self);
    }
  self.window = nil;
  [super dealloc];
  }
@end

@interface AppDelegate : NSObject <UIApplicationDelegate> {
  uint64_t activationGeneration;
  bool     legacyWindow;
  }
@end

@implementation AppDelegate
- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)launchOptions {
  (void)application;
  (void)launchOptions;
  if(@available(iOS 13.0, *)) {
    usesSceneLifecycle = [[NSBundle mainBundle]
        objectForInfoDictionaryKey:@"UIApplicationSceneManifest"]!=nil;
    }
  legacyWindow = !usesSceneLifecycle;
  return YES;
  }

- (void)applicationDidBecomeActive:(UIApplication *)application {
  (void)application;
  if(!legacyWindow)
    return;
  activateWindow(self,@selector(resumeEngineIfCurrent:),
                 activationGeneration,mainWindow);
  }

- (void)applicationWillResignActive:(UIApplication *)application {
  (void)application;
  if(!legacyWindow)
    return;
  deactivateWindow(self,activationGeneration,mainWindow);
  }

- (void)resumeEngineIfCurrent:(NSNumber*)generation {
  if(!legacyWindow)
    return;
  ::resumeEngineIfCurrent(generation,activationGeneration,mainWindow);
  }

- (UISceneConfiguration *)application:(UIApplication *)application
    configurationForConnectingSceneSession:(UISceneSession *)connectingSceneSession
    options:(UISceneConnectionOptions *)options API_AVAILABLE(ios(13.0)) {
  (void)application;
  (void)options;
  if(!usesSceneLifecycle) {
    if(legacyWindow)
      deactivateWindow(self,activationGeneration,mainWindow);
    legacyWindow = false;
    usesSceneLifecycle = true;
    }
  UISceneConfiguration* configuration = connectingSceneSession.configuration;
  if(configuration==nil)
    configuration = [UISceneConfiguration configurationWithName:nil
                                                     sessionRole:connectingSceneSession.role];
  configuration.delegateClass = [TempestSceneDelegate class];
  return configuration;
  }

- (UIInterfaceOrientationMask)application:(UIApplication *)application
  supportedInterfaceOrientationsForWindow:(UIWindow *)window {
  return UIInterfaceOrientationMaskAll;
  }

- (void)applicationWillTerminate:(UIApplication *)application {
  (void)application;
  cancelEngineResume(self);
  ++lifecycleGeneration;
  activationResumePending = false;
  isApplicationActive.store(false);
  isEngineReady.store(false);
  isRunning.store(false);
  if(mainWindow!=nil) {
    invalidateDisplayLink(mainWindow);
    mainWindow->owner = nullptr;
    discardPendingEvent(mainWindow);
    mainWindow->touch.clear();
    if(@available(iOS 13.0, *))
      mainWindow.windowScene = nil;
    [mainWindow release];
    mainWindow = nil;
    }
  activeSceneDelegate = nil;
  if(@available(iOS 13.0, *))
    activeWindowScene = nil;
  }
@end


struct Fiber  {
  jmp_buf jmp = {};
  };

static Fiber            mainContext;
static Fiber            appleContext;
static Fiber*           currentContext = nullptr;
alignas(16) static char appleStack[1*1024*1024]={};
// createAppleSubContext changes sp before this call. Keep appleMain separate so
// its locals are allocated below the top of appleStack, not above the buffer.
__attribute__((noinline)) static void appleMain(void*);

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
  if(window!=nil && window->owner!=nullptr)
    return nullptr;

  if(window==nil) {
    if(usesSceneLifecycle) {
      if(@available(iOS 13.0, *)) {
        if(activeSceneDelegate==nil || activeWindowScene==nil)
          return nullptr;
        window = [[TempestWindow alloc] initWithWindowScene:activeWindowScene];
        }
      else {
        return nullptr;
        }
      }
    else {
      window = [[TempestWindow alloc] initWithFrame:[UIScreen mainScreen].bounds];
      }

    if(!initializeWindow(window)) {
      [window release];
      return nullptr;
      }

    if(usesSceneLifecycle) {
      if(@available(iOS 13.0, *)) {
        if(!attachWindowToScene(window,activeWindowScene)) {
          [window release];
          return nullptr;
          }
        }
      }
    else {
      window.contentScaleFactor = [UIScreen mainScreen].scale;
      }
    mainWindow = window;
    }
  else if(usesSceneLifecycle) {
    if(@available(iOS 13.0, *)) {
      if(activeSceneDelegate==nil || activeWindowScene==nil ||
         !attachWindowToScene(window,activeWindowScene))
        return nullptr;
      }
    else {
      return nullptr;
      }
    }

  if(usesSceneLifecycle) {
    if(@available(iOS 13.0, *))
      activeSceneDelegate.window = window;
    }

  [window makeKeyAndVisible];

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
  wx->owner = nullptr;
  invalidateDisplayLink(wx);
  discardPendingEvent(wx);
  wx->touch.clear();
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
  
  // UIKit already owns an autorelease pool around each event-loop iteration.
  // A nested processEvents() can let UIKit drain that outer pool before this
  // fiber frame resumes, so do not push a pool whose lifetime spans callbacks.
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
