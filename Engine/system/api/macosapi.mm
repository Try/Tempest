#include "macosapi.h"

#include <AppKit/AppKit.h>

using namespace Tempest;

static SystemApi::Window* createWindow(uint32_t w, uint32_t h, uint flags, SystemApi::ShowMode mode) {
    NSWindow* wnd = [[NSWindow alloc] initWithContentRect:
      NSMakeRect(0, 0, w, h)
    styleMask:NSTitledWindowMask
    backing:NSBackingStoreBuffered
    defer:NO];
    [wnd cascadeTopLeftFromPoint:NSMakePoint(20,20)];
    [wnd setTitle:@"Tempest"];
    [wnd makeKeyAndOrderFront:nil];
    [wnd setStyleMask:[wnd styleMask] | flags];
    [wnd setAcceptsMouseMovedEvents: YES];

    switch (mode) {
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
  return reinterpret_cast<SystemApi::Window*>(wnd);
  }

MacOSApi::MacOSApi() {
  }

SystemApi::Window *MacOSApi::implCreateWindow(Tempest::Window *owner, uint32_t width, uint32_t height, SystemApi::ShowMode sm) {
  return ::createWindow(width,height,
                          NSWindowStyleMaskTitled |
                          NSWindowStyleMaskClosable |
                          NSWindowStyleMaskMiniaturizable |
                          NSWindowStyleMaskResizable,
                        sm);
  }

SystemApi::Window *MacOSApi::implCreateWindow(Tempest::Window *owner, uint32_t width, uint32_t height) {

  }

SystemApi::Window *MacOSApi::implCreateWindow(Tempest::Window *owner, SystemApi::ShowMode sm) {

  }

void MacOSApi::implDestroyWindow(SystemApi::Window *w) {

  }

void MacOSApi::implExit() {

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
    return true;
  }

int MacOSApi::implExec(SystemApi::AppCallBack &cb) {

  }

void MacOSApi::implProcessEvents(SystemApi::AppCallBack &cb) {

  }
