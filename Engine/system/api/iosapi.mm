#include "iosapi.h"
#include <Tempest/Platform>

#ifdef __IOS__

#import <UIKit/UIKit.h>
#include <string>
#include <thread>
#include <TargetConditionals.h>

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
  }
@end
@implementation TempestWindow
- (void)layoutSubviews {
  [super layoutSubviews];
  }
- (void)drawFrame {
  hasPendingFrame.store(true);
  //swapContext();
  drawFrame();
  }
@end

static TempestWindow* mainWindow = nullptr;


@interface ViewController:UIViewController{}
@end

@implementation ViewController
- (void)viewDidLoad {
    [self prefersStatusBarHidden];
  }

- (BOOL)prefersStatusBarHidden {
  return YES;
  }

- (BOOL) shouldAutorotate {
  return YES;
  }

@end

@interface AppDelegate : NSObject <UIApplicationDelegate> {
  }
  @end

@implementation AppDelegate

- (BOOL)application:(UIApplication *)application didFinishLaunchingWithOptions:(NSDictionary *)launchOptions {
  (void)application;
  (void)launchOptions;

  CGRect frame = [ [ UIScreen mainScreen ] bounds ];
  TempestWindow *window = [ [ TempestWindow alloc ] initWithFrame: frame];
  window.contentScaleFactor = [UIScreen mainScreen].scale;
  window.rootViewController = [ViewController new];
  window.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;

  window.backgroundColor = [ UIColor blueColor ];

  window->owner = nullptr;
  window->displayLink = nullptr;
  window->hasPendingFrame.store(false);
  
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
  }


- (void)applicationDidEnterBackground:(UIApplication *)application {
    (void)application;
  }


- (void)applicationWillEnterForeground:(UIApplication *)application {
    (void)application;
  }


- (void)applicationDidBecomeActive:(UIApplication *)application  {
    (void)application;
    swapContext();
  }


- (void)applicationWillTerminate:(UIApplication *)application {
    (void)application;
  }

@end


struct Fiber  {
  ucontext_t  fib;
  jmp_buf     jmp;
  };

struct FiberCtx  {
  void(*      fnc)(void*);
  void*       ctx;
  jmp_buf*    cur;
  ucontext_t* prv;
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

    /*
    __asm__ __volatile__("mov lr, %0"
      :
      : "r" (alignDown(0llu, FUNCTION_CALL_ALIGNMENT)));*/

    __asm__ __volatile__(
                SET_STACK_POINTER
                : // no outputs
                : "r" (alignDown(base, FUNCTION_CALL_ALIGNMENT))
            );

    currentContext = &appleContext;
    appleMain(nullptr);
    }
  }

inline static void swapContext() {
  if(currentContext==&appleContext) {
    currentContext = &mainContext;
    if(_setjmp(appleContext.jmp) == 0)
      _longjmp(mainContext.jmp, 1);
    } else {
    currentContext = &appleContext;
    if(_setjmp(mainContext.jmp) == 0)
      _longjmp(appleContext.jmp, 1);
    }
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
    &app[0],nullptr
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
  CGRect rect = [ [ UIScreen mainScreen ] bounds ];
  return Rect(0,0,rect.size.width,rect.size.height);
  }

bool iOSApi::implSetAsFullscreen(Window* w, bool fullScreen) {
  return false;
  }

bool iOSApi::implIsFullscreen(Window* w) {

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
    }
  return 0;
  }

void iOSApi::implProcessEvents(AppCallBack& cb) {
  if(mainWindow!=nil && mainWindow->hasPendingFrame.load()) {
    auto cb = (mainWindow->owner);
    @autoreleasepool {
      if(cb!=nullptr) {
        mainWindow->hasPendingFrame.store(false);
        iOSApi::dispatchRender(*cb);
        }
      }
    swapContext();
    return;
    }
  swapContext();
  }

void iOSApi::implSetWindowTitle(Window* w, const char* utf8) {

  }

#endif
