#include "macosapi.h"

#include <Carbon/Carbon.h>
#include <AppKit/AppKit.h>

#include <Tempest/Window>
#include <Tempest/Log>

using namespace Tempest;

static std::atomic_bool isRunning{true};

static Event::MouseButton toButton(uint type) {
  if(type==NSEventTypeLeftMouseDown || type==NSEventTypeLeftMouseUp)
    return Event::ButtonLeft;
  if(type==NSEventTypeRightMouseDown || type==NSEventTypeRightMouseUp)
    return Event::ButtonRight;
  if(type==NSEventTypeOtherMouseDown || type==NSEventTypeOtherMouseUp)
    return Event::ButtonMid;
  return Event::ButtonNone;
  }

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
  static const TranslateKeyPair k[] = {
    { kVK_Control,       Event::K_LControl },
    { kVK_RightControl,  Event::K_RControl },

    { kVK_Shift,         Event::K_LShift   },
    { kVK_RightShift,    Event::K_RShift   },

    { kVK_Option,        Event::K_LAlt     },
    { kVK_RightOption,   Event::K_RAlt     },

    { kVK_LeftArrow,     Event::K_Left     },
    { kVK_RightArrow,    Event::K_Right    },
    { kVK_UpArrow,       Event::K_Up       },
    { kVK_DownArrow,     Event::K_Down     },

    { kVK_Escape,        Event::K_ESCAPE   },
    { kVK_Delete,        Event::K_Back     },
    { kVK_Tab,           Event::K_Tab      },
    { kVK_ForwardDelete, Event::K_Delete   },
    //{ kVK_Insert,        Event::K_Insert   },
    { kVK_Home,          Event::K_Home     },
    { kVK_End,           Event::K_End      },
    //{ kVK_Pause,         Event::K_Pause    },
    { kVK_Return,        Event::K_Return   },
    { kVK_Space,         Event::K_Space    },
    { kVK_CapsLock,      Event::K_CapsLock },

    { kVK_F1,            Event::K_F1       },
    { kVK_ANSI_0,        Event::K_0        },
    { kVK_ANSI_A,        Event::K_A        },
    { 0,                 Event::K_NoKey    }
    };

  setupKeyTranslate(k,20);

  [NSApplication sharedApplication];

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

  [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
  [NSApp activateIgnoringOtherApps:YES];
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

  NSWindow* wnd = evt.window;
  if(wnd==nil) {
    [NSApp sendEvent: evt];
    return;
    }

  id<NSWindowDelegate> d = wnd.delegate;
  if(![d isKindOfClass:[TempestWindowDelegate class]]) {
    [NSApp sendEvent: evt];
    return;
    }

  TempestWindowDelegate* dx = d;
  Tempest::Window&       cb = *dx->owner;

  NSEventType type = [evt type];
  switch(type) {
    case NSEventTypeLeftMouseDown:
    case NSEventTypeRightMouseDown:
    case NSEventTypeOtherMouseDown:{
      MouseEvent e(evt.locationInWindow.x,
                   evt.locationInWindow.y,
                   toButton(type),
                   Event::M_NoModifier,
                   0,
                   0,
                   Event::MouseDown
                   );
      SystemApi::dispatchMouseDown(cb,e);
      break;
      }

    case NSEventTypeLeftMouseUp:
    case NSEventTypeRightMouseUp:
    case NSEventTypeOtherMouseUp:{
      MouseEvent e(evt.locationInWindow.x,
                   evt.locationInWindow.y,
                   toButton(type),
                   Event::M_NoModifier,
                   0,
                   0,
                   Event::MouseUp
                   );
      SystemApi::dispatchMouseUp(cb,e);
      break;
      }
    case NSEventTypeLeftMouseDragged:
    case NSEventTypeRightMouseDragged:
    case NSEventTypeOtherMouseDragged:
    case NSEventTypeMouseMoved: {
      MouseEvent e(evt.locationInWindow.x,
                   evt.locationInWindow.y,
                   toButton(type),
                   Event::M_NoModifier,
                   0,
                   0,
                   Event::MouseMove
                   );
      SystemApi::dispatchMouseMove(cb,e);
      break;
      }
    case NSEventTypeScrollWheel:{
      float ticks = [evt scrollingDeltaY];
      MouseEvent e(evt.locationInWindow.x,
                   evt.locationInWindow.y,
                   toButton(type),
                   Event::M_NoModifier,
                   int32_t(ticks),
                   0,
                   Event::MouseWheel
                   );
      SystemApi::dispatchMouseWheel(cb,e);
      break;
      }
    case NSEventTypeKeyDown:
    case NSEventTypeKeyUp: {
      Event::Type    eType = (type==NSEventTypeKeyDown ? Event::KeyDown : Event::KeyUp);
      unsigned short key   = [evt  keyCode];
      NSString*      text  = [evt  characters];
      const char*    utf8  = [text UTF8String];

      KeyEvent::KeyType kt = KeyEvent::K_NoKey;
      switch(key){
        case kVK_ANSI_0: kt = KeyEvent::K_0; break;
        case kVK_ANSI_1: kt = KeyEvent::K_1; break;
        case kVK_ANSI_2: kt = KeyEvent::K_2; break;
        case kVK_ANSI_3: kt = KeyEvent::K_3; break;
        case kVK_ANSI_4: kt = KeyEvent::K_4; break;
        case kVK_ANSI_5: kt = KeyEvent::K_5; break;
        case kVK_ANSI_6: kt = KeyEvent::K_6; break;
        case kVK_ANSI_7: kt = KeyEvent::K_7; break;
        case kVK_ANSI_8: kt = KeyEvent::K_8; break;
        case kVK_ANSI_9: kt = KeyEvent::K_9; break;
        case kVK_F1:     kt = KeyEvent::K_F1; break;
        case kVK_F2:     kt = KeyEvent::K_F2; break;
        case kVK_F3:     kt = KeyEvent::K_F3; break;
        case kVK_F4:     kt = KeyEvent::K_F4; break;
        case kVK_F5:     kt = KeyEvent::K_F5; break;
        case kVK_F6:     kt = KeyEvent::K_F6; break;
        case kVK_F7:     kt = KeyEvent::K_F7; break;
        case kVK_F8:     kt = KeyEvent::K_F8; break;
        case kVK_F9:     kt = KeyEvent::K_F9; break;
        case kVK_F10:    kt = KeyEvent::K_F10; break;
        case kVK_F11:    kt = KeyEvent::K_F11; break;
        case kVK_F12:    kt = KeyEvent::K_F12; break;
        case kVK_F13:    kt = KeyEvent::K_F13; break;
        case kVK_F14:    kt = KeyEvent::K_F14; break;
        case kVK_F15:    kt = KeyEvent::K_F15; break;
        case kVK_F16:    kt = KeyEvent::K_F16; break;
        case kVK_F17:    kt = KeyEvent::K_F17; break;
        case kVK_F18:    kt = KeyEvent::K_F18; break;
        case kVK_F19:    kt = KeyEvent::K_F19; break;
        case kVK_F20:    kt = KeyEvent::K_F20; break;
        default: kt = Event::KeyType(SystemApi::translateKey(key));
        }
      // TODO
      break;
      }
    case NSEventTypeFlagsChanged:{
      // TODO
      break;
      }
    case NSEventTypeAppKitDefined:{
      NSEventSubtype stype = evt.subtype;
      if(stype==NSEventSubtypeScreenChanged || stype==NSEventSubtypeWindowMoved) {
        NSRect r = [wnd frame];
        SizeEvent sz{int32_t(r.size.width),int32_t(r.size.height)};
        SystemApi::dispatchResize(cb,sz);
        }
      break;
      }

    default:
      SystemApi::dispatchRender(cb);
    }

  [NSApp sendEvent: evt];
  }
