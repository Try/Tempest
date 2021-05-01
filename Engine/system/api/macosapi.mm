#include "macosapi.h"

#include <AppKit/AppKit.h>
#include <Tempest/Window>

using namespace Tempest;

static std::atomic_bool isRunning{true};

@interface TempestWindowDelegate : NSObject<NSWindowDelegate> {
  @public Tempest::Window* owner;
  }
@end

@implementation TempestWindowDelegate
- (void)windowWillClose:(id)sender {
  isRunning.store(false);
  }
@end

static SystemApi::Window* createWindow(Tempest::Window *owner,
                                       uint32_t w, uint32_t h, uint flags, SystemApi::ShowMode mode) {
  NSWindow* wnd = [[NSWindow alloc] initWithContentRect:
      NSMakeRect(0, 0, w, h)
    styleMask:NSWindowStyleMaskTitled
    backing:NSBackingStoreBuffered
    defer:NO];

  [wnd cascadeTopLeftFromPoint:NSMakePoint(20,20)];
  [wnd setTitle:@"Tempest"];
  [wnd makeKeyAndOrderFront:nil];
  [wnd setStyleMask:[wnd styleMask] | flags];
  [wnd setAcceptsMouseMovedEvents: YES];

  switch (mode) {
    case SystemApi::Hidden: break;
    case SystemApi::Normal: break;
    case SystemApi::Minimized:
      [wnd miniaturize:wnd];
      break;
    case SystemApi::Maximized:
      [wnd setFrame:[[NSScreen mainScreen] visibleFrame] display:YES];
      break;
    case SystemApi::FullScreen:
      [wnd toggleFullScreen:wnd];
      break;
    }

  TempestWindowDelegate* delegate = [[TempestWindowDelegate alloc] init];
  delegate->owner = owner;
  [wnd setDelegate: delegate];

  [wnd orderFrontRegardless];

  return reinterpret_cast<SystemApi::Window*>(wnd);
  }

MacOSApi::MacOSApi() {
  NSMenu*      bar     = [NSMenu new];
  NSMenuItem * barItem = [NSMenuItem new];
  NSMenu*      menu    = [NSMenu new];
  NSMenuItem*  quit    = [[NSMenuItem alloc]
                         initWithTitle:@"Quit"
                         action:@selector(terminate:)
                         keyEquivalent:@"q"];
  [bar     addItem:barItem];
  [barItem setSubmenu:menu];
  [menu    addItem:quit];
  NSApp.mainMenu = bar;
  }

SystemApi::Window *MacOSApi::implCreateWindow(Tempest::Window *owner, uint32_t width, uint32_t height,
                                              SystemApi::ShowMode sm) {
  return ::createWindow(owner,width,height,
                        NSWindowStyleMaskTitled |
                        NSWindowStyleMaskClosable |
                        NSWindowStyleMaskMiniaturizable |
                        NSWindowStyleMaskResizable,
                        sm);
  }

SystemApi::Window *MacOSApi::implCreateWindow(Tempest::Window *owner, uint32_t width, uint32_t height) {
  return implCreateWindow(owner,width,height,ShowMode::Normal);
  }

SystemApi::Window *MacOSApi::implCreateWindow(Tempest::Window *owner, SystemApi::ShowMode sm) {
  return implCreateWindow(owner,800,600,sm);
  }

void MacOSApi::implDestroyWindow(SystemApi::Window *w) {
  NSWindow* wnd = reinterpret_cast<NSWindow*>(w);
  [wnd release];
  }

void MacOSApi::implExit() {
  ::isRunning.store(false);
  }

Tempest::Rect MacOSApi::implWindowClientRect(SystemApi::Window *w) {
  NSWindow* wnd = reinterpret_cast<NSWindow*>(w);
  NSRect frame = [wnd frame];
  frame = [wnd contentRectForFrameRect:frame];
  return Rect(frame.origin.x,frame.origin.y,frame.size.width,frame.size.height);
  }

bool MacOSApi::implSetAsFullscreen(SystemApi::Window *w, bool fullScreen) {
  return false;
  }

bool MacOSApi::implIsFullscreen(SystemApi::Window *w) {
  return false;
  }

void MacOSApi::implSetCursorPosition(SystemApi::Window *w, int x, int y) {

  }

void MacOSApi::implShowCursor(bool show) {

  }

bool MacOSApi::implIsRunning() {
  return ::isRunning.load();
  }

int MacOSApi::implExec(SystemApi::AppCallBack &cb) {
  while(::isRunning.load()) {
    implProcessEvents(cb);
    }
  return 0;
  }

void MacOSApi::implProcessEvents(SystemApi::AppCallBack&) {
  NSEvent* evt = [NSApp nextEventMatchingMask: NSEventMaskAny
                 untilDate: nil
                 inMode: NSDefaultRunLoopMode
                 dequeue: YES];

  switch([evt type]) {
    default: {
      NSWindow* wnd = evt.window;
      if(wnd!=nil) {
        id<NSWindowDelegate> d = wnd.delegate;
        if([d isKindOfClass:[TempestWindowDelegate class]]) {
          TempestWindowDelegate* dx = d;
          SystemApi::dispatchRender(*dx->owner);
          }
        }
      [NSApp sendEvent: evt];
      }
    }
  }
