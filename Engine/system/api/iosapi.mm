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

  if(owner==nullptr)
    return;
  
  new (&event.size) SizeEvent(int32_t(frame.size.width*scale), int32_t(frame.size.height*scale));
  curentEvent = Event::Resize;
  swapContext();
  }

- (void)drawFrame {
  hasPendingFrame.store(true);
  swapContext();
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
    swapContext();
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
    swapContext();
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
    swapContext();
    }
  }
@end

static TempestWindow* mainWindow = nullptr;


@interface ViewController:UIViewController{}
-(id)init;
@end

@implementation ViewController {
  bool fullScreen;
  }

-(id)init {
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

@interface AppDelegate : NSObject <UIApplicationDelegate> {
  }
@end

static bool isApplicationActive = false;

@implementation AppDelegate
- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)launchOptions {
  (void)application;
  (void)launchOptions;

  CGRect frame = [ [ UIScreen mainScreen ] bounds ];
  TempestWindow  * window = [ [ TempestWindow alloc ] initWithFrame: frame];
  window.contentScaleFactor = [UIScreen mainScreen].scale;
  window.rootViewController = [ViewController new];
  window.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
  window.backgroundColor = [ UIColor blackColor ];

  window->owner = nullptr;
  window->displayLink = nullptr;
  window->hasPendingFrame.store(false);
  window->curentEvent = Event::Type::NoEvent;
  
  mainWindow = window;
  [ window makeKeyAndVisible ]; // possible switch here
  return YES;
  }

- (UIInterfaceOrientationMask)application:(UIApplication *)application
  supportedInterfaceOrientationsForWindow:(UIWindow *)window {
  return UIInterfaceOrientationMaskAll;
  }

- (void)applicationWillResignActive:(UIApplication *)application {
  (void)application;
  isApplicationActive = false;
  swapContext();
  }

- (void)applicationDidEnterBackground:(UIApplication *)application {
  (void)application;
  }

- (void)applicationWillEnterForeground:(UIApplication *)application {
  (void)application;
  }

- (void)applicationDidBecomeActive:(UIApplication *)application  {
  (void)application;
  isApplicationActive = true;
  swapContext();
  }

- (void)applicationWillTerminate:(UIApplication *)application {
  (void)application;
  }
@end


struct Fiber  {
  jmp_buf jmp = {};
  };

static std::atomic_bool isRunning{true};
static Fiber            mainContext;
static Fiber            appleContext;
static Fiber*           currentContext = nullptr;
alignas(16) static char appleStack[1*1024*1024]={};
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
  auto window = mainWindow;
  
  window->owner = owner;
  window->displayLink = [CADisplayLink displayLinkWithTarget:window selector:@selector(drawFrame)];
  //by adding the display link to the run loop our draw method will be called 60 times per second
  [window->displayLink addToRunLoop:[NSRunLoop currentRunLoop] forMode:NSDefaultRunLoopMode];
  window->hasPendingFrame.store(true);
  
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
  }

void iOSApi::implExit() {
  ::isRunning.store(false);
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
  
  @autoreleasepool {
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
        if(isApplicationActive && mainWindow->hasPendingFrame.load()) {
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
