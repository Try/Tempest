#include "macosapi.h"

#include <Carbon/Carbon.h>
#include <AppKit/AppKit.h>

#include <Tempest/Window>
#include <Tempest/Log>
#include <Tempest/TextCodec>

using namespace Tempest;

static const uint keyTable[26]={
  kVK_ANSI_A,
  kVK_ANSI_B,
  kVK_ANSI_C,
  kVK_ANSI_D,
  kVK_ANSI_E,
  kVK_ANSI_F,
  kVK_ANSI_G,
  kVK_ANSI_H,
  kVK_ANSI_I,
  kVK_ANSI_J,
  kVK_ANSI_K,
  kVK_ANSI_L,
  kVK_ANSI_M,
  kVK_ANSI_N,
  kVK_ANSI_O,
  kVK_ANSI_P,
  kVK_ANSI_Q,
  kVK_ANSI_R,
  kVK_ANSI_S,
  kVK_ANSI_T,
  kVK_ANSI_U,
  kVK_ANSI_V,
  kVK_ANSI_W,
  kVK_ANSI_X,
  kVK_ANSI_Y,
  kVK_ANSI_Z
  };

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

static NSPoint mousePos(NSPoint px, NSWindow* wnd, bool& inWindow) {
  NSRect fr = wnd.contentView.frame;
  fr = [wnd contentRectForFrameRect:fr];

  px = [wnd convertPointToBacking:px];
  fr = [wnd convertRectToBacking: fr];

  px.y = fr.size.height - px.y;

  px.x -= fr.origin.x;
  px.y -= fr.origin.y;

  if(0<=px.x && px.x<fr.size.width &&
     0<=px.y && px.y<fr.size.height)
    inWindow = true; else
    inWindow = false;

  return px;
  }

static Tempest::Point mousePos(NSEvent* e, bool& inWindow) {
  static NSPoint err = {};
  NSPoint p  = e.locationInWindow;
  NSPoint px = mousePos(p, e.window, inWindow);

  px.x += err.x;
  px.y += err.y;

  err.x = std::fmod(px.x,1.0);
  err.y = std::fmod(px.y,1.0);

  return Tempest::Point{int(px.x), int(px.y)};
  }

static Tempest::Point mousePos(NSEvent* e) {
  bool dummy = false;
  return mousePos(e,dummy);
  }

void Detail::ImplMacOSApi::onDisplayLink(void* hwnd) {
  @autoreleasepool {
    auto cb = reinterpret_cast<Tempest::Window*>(hwnd);
    MacOSApi::dispatchRender(*cb);
    }
  }

void Detail::ImplMacOSApi::onDidResize(void* hwnd, void* w) {
  auto      cb  = reinterpret_cast<Tempest::Window*>(hwnd);
  NSWindow* wnd = reinterpret_cast<NSWindow*>(w);

  NSRect r = [wnd convertRectToBacking: [wnd frame]];
  SizeEvent sz{int32_t(r.size.width),int32_t(r.size.height)};
  MacOSApi::dispatchResize(*cb,sz);
  }

void Detail::ImplMacOSApi::onDidBecomeKey(void* hwnd, void* w) {
  auto      cb  = reinterpret_cast<Tempest::Window*>(hwnd);
  NSWindow* wnd = reinterpret_cast<NSWindow*>(w);

  FocusEvent e(true, Event::UnknownReason);
  MacOSApi::dispatchFocus(*cb, e);
  }

void Detail::ImplMacOSApi::onDidResignKey(void* hwnd, void* w) {
  auto      cb  = reinterpret_cast<Tempest::Window*>(hwnd);
  NSWindow* wnd = reinterpret_cast<NSWindow*>(w);

  FocusEvent e(false, Event::UnknownReason);
  MacOSApi::dispatchFocus(*cb, e);
  }

@interface TempestWindowDelegate : NSObject<NSWindowDelegate> {
  @public Tempest::Window* owner;
  @public CVDisplayLinkRef displayLink;
  @public std::atomic_bool hasPendingFrame;
  }
- (void)dispatchRenderer;
@end

static CVReturn displayLinkCallback(CVDisplayLinkRef /*displayLink*/,
                                    const CVTimeStamp* /*now*/, const CVTimeStamp* /*outputTime*/,
                                    CVOptionFlags /*flagsIn*/, CVOptionFlags* /*flagsOut*/,
                                    void* displayLinkContext) {
  TempestWindowDelegate* self = (__bridge TempestWindowDelegate*)displayLinkContext;
  bool f = false;
  if(!self->hasPendingFrame.compare_exchange_strong(f,true))
    return kCVReturnSuccess;
  // Need to dispatch to main thread as CVDisplayLink uses it's own thread.
  dispatch_async(dispatch_get_main_queue(), ^{
                 [self dispatchRenderer];
                 });
  return kCVReturnSuccess;
 }

@implementation TempestWindowDelegate
- (id)init {
  self = [super init];
  hasPendingFrame.store(0);
  // Setup display link.
  CVDisplayLinkCreateWithActiveCGDisplays(&displayLink);
  CVDisplayLinkSetOutputCallback(displayLink, &displayLinkCallback, (__bridge void*)self);
  CVDisplayLinkStart(displayLink);
  return self;
  }

- (void)dealloc {
  CVDisplayLinkStop(displayLink);
  [super dealloc];
  }

- (void)windowWillClose:(id)sender {
  isRunning.store(false);
  }

- (void)windowDidBecomeKey:(NSNotification *)notification {
  NSWindow* wnd  = [notification object];
  Detail::ImplMacOSApi::onDidBecomeKey(owner,wnd);
  }

- (void)windowDidResignKey:(NSNotification *)notification {
  NSWindow* wnd  = [notification object];
  Detail::ImplMacOSApi::onDidResignKey(owner,wnd);
  }

- (void)windowDidResize:(NSNotification*)notification {
  NSWindow* wnd  = [notification object];
  Detail::ImplMacOSApi::onDidResize(owner,wnd);
  }

- (void)dispatchRenderer{
  Detail::ImplMacOSApi::onDisplayLink(owner);
  hasPendingFrame.store(false);
  }
@end

static SystemApi::Window* createWindow(Tempest::Window *owner,
                                       uint32_t w, uint32_t h, uint flags, SystemApi::ShowMode mode) {
  // TODO: hDPI
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
  [wnd setCollectionBehavior:NSWindowCollectionBehaviorFullScreenPrimary];

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

  TempestWindowDelegate* delegate = [TempestWindowDelegate new];
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
  NSWindow*              wnd   = reinterpret_cast<NSWindow*>(w);
  TempestWindowDelegate* deleg = wnd.delegate;
  [wnd   release];
  [deleg release];
  }

void MacOSApi::implExit() {
  ::isRunning.store(false);
  }

Tempest::Rect MacOSApi::implWindowClientRect(SystemApi::Window *w) {
  NSWindow* wnd = reinterpret_cast<NSWindow*>(w);
  NSRect frame = [wnd frame];
  frame = [wnd contentRectForFrameRect:frame];
  frame = [wnd convertRectToBacking:frame];
  return Rect(frame.origin.x,frame.origin.y,frame.size.width,frame.size.height);
  }

bool MacOSApi::implSetAsFullscreen(SystemApi::Window *w, bool fullScreen) {
  NSWindow* wnd = reinterpret_cast<NSWindow*>(w);
  const bool isFs = implIsFullscreen(w);
  if(isFs==fullScreen)
    return true;
  [wnd toggleFullScreen:nil];
  return ([wnd styleMask] & NSWindowStyleMaskFullScreen)==fullScreen;
  }

bool MacOSApi::implIsFullscreen(SystemApi::Window *w) {
  NSWindow* wnd = reinterpret_cast<NSWindow*>(w);
  return ([wnd styleMask] & NSWindowStyleMaskFullScreen);
  }

void MacOSApi::implSetCursorPosition(SystemApi::Window *w, int x, int y) {
  NSWindow* wnd      = reinterpret_cast<NSWindow*>(w);
  NSRect    frame    = [wnd frame];
  frame = [wnd convertRectToBacking:frame];

  NSPoint to = {CGFloat(x),CGFloat(y)};
  to.x += frame.origin.x;
  to.y += frame.origin.y;
  to.y =  frame.size.height-to.y;

  NSPoint p = [wnd convertPointFromBacking:to];
  p = [wnd convertPointToScreen:p];

  CGWarpMouseCursorPosition(p);
  // see: https://www.winehq.org/pipermail/wine-patches/2015-October/143826.html
  CGAssociateMouseAndMouseCursorPosition(true);
  }

void MacOSApi::implShowCursor(SystemApi::Window *w, CursorShape show) {
  if(show==CursorShape::Hidden) {
    CGDisplayHideCursor(kCGNullDirectDisplay);
    return;
    }
  CGDisplayShowCursor(kCGNullDirectDisplay);
  }

bool MacOSApi::implIsRunning() {
  return ::isRunning.load();
  }

int MacOSApi::implExec(SystemApi::AppCallBack &cb) {
  while(::isRunning.load()) {
    implProcessEvents(cb);
    if(!cb.onTimer())
      std::this_thread::yield();
    }
  return 0;
  }

void MacOSApi::implProcessEvents(SystemApi::AppCallBack&) {
  NSEvent* evt = [NSApp nextEventMatchingMask: NSEventMaskAny
                 untilDate: nil
                 inMode: NSDefaultRunLoopMode
                 dequeue: YES];
  if(evt==nil)
    return;
  NSEventType type = [evt type];
  NSWindow*   wnd  = evt.window;

  if(wnd==nil) {
    [NSApp sendEvent: evt];
    std::this_thread::yield();
    return;
    }

  id<NSWindowDelegate> d = wnd.delegate;
  if(![d isKindOfClass:[TempestWindowDelegate class]]) {
    [NSApp sendEvent: evt];
    std::this_thread::yield();
    return;
    }

  TempestWindowDelegate* dx = d;
  Tempest::Window&       cb = *dx->owner;

  switch(type) {
    case NSEventTypeLeftMouseDown:
    case NSEventTypeRightMouseDown:
    case NSEventTypeOtherMouseDown:{
      bool inWindow = false;
      auto p = mousePos(evt,inWindow);
      if(!inWindow)
        break;
      MouseEvent e(p.x,p.y,
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
      auto p = mousePos(evt);
      MouseEvent e(p.x,p.y,
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
      auto p = mousePos(evt);
      MouseEvent e(p.x,p.y,
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
      float ticks    = [evt deltaY];
      int   ticksI   = (ticks>0 ? 120 : (ticks<0 ? -120 : 0));
      bool  inWindow = false;
      auto  p        = mousePos(evt,inWindow);
      if(!inWindow)
        break;
      MouseEvent e(p.x,p.y,
                   toButton(type),
                   Event::M_NoModifier,
                   ticksI,
                   0,
                   Event::MouseWheel
                   );
      SystemApi::dispatchMouseWheel(cb,e);
      break;
      }
    case NSEventTypeKeyDown:
    case NSEventTypeKeyUp: {
      uint16_t       type  = [evt type];
      Event::Type    eType = (type==NSEventTypeKeyDown ? Event::KeyDown : Event::KeyUp);
      const uint16   key   = [evt  keyCode];
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

      for(const uint& k:keyTable)
        if(k==key)
          kt = Event::KeyType(Event::K_A+(&k-keyTable));

      auto u16 = TextCodec::toUtf16(utf8); // TODO: remove dynamic allocation
      auto k   = KeyEvent(kt,uint32_t(u16.size()>0 ? u16[0] : 0),Event::M_NoModifier,eType);
      if(k.type()==Event::KeyDown)
        SystemApi::dispatchKeyDown(cb,k,key); else
        SystemApi::dispatchKeyUp(cb,k,key);
      return;
      }
    case NSEventTypeFlagsChanged: {
      NSEventModifierFlags flag     = 0;
      KeyEvent::KeyType    kt       = KeyEvent::K_NoKey;
      Event::Modifier      modifier = Event::M_NoModifier;

      switch(evt.keyCode) {
        case kVK_Command: {
          flag     = NSEventModifierFlagCommand;
          kt       = KeyEvent::K_LCommand;
          modifier = Event::M_Command;
          break;
          }
        case kVK_Shift: {
          flag     = NSEventModifierFlagShift;
          kt       = KeyEvent::K_LShift;
          modifier = Event::M_Shift;
          break;
          }
        case kVK_Option: {
          flag     = NSEventModifierFlagOption;
          kt       = KeyEvent::K_LAlt;
          modifier = Event::M_Alt;
          break;
          }
        case kVK_Control: {
          flag     = NSEventModifierFlagControl;
          kt       = KeyEvent::K_LControl;
          modifier = Event::M_Ctrl;
          break;
          }
        case kVK_RightCommand: {
          flag     = NSEventModifierFlagCommand;
          kt       = KeyEvent::K_RCommand;
          modifier = Event::M_Command;
          break;
          }
        case kVK_RightShift: {
          flag     = NSEventModifierFlagShift;
          kt       = KeyEvent::K_RShift;
          modifier = Event::M_Shift;
          break;
          }
        case kVK_RightOption: {
          flag     = NSEventModifierFlagOption;
          kt       = KeyEvent::K_RAlt;
          modifier = Event::M_Alt;
          break;
          }
        case kVK_RightControl: {
          flag     = NSEventModifierFlagControl;
          kt       = KeyEvent::K_RControl;
          modifier = Event::M_Ctrl;
          break;
          }
        default: break;
		}

      auto isDown = evt.modifierFlags & flag;
      auto eType  = (isDown ? Event::KeyDown : Event::KeyUp);
      auto k      = KeyEvent(kt,modifier,eType);

      if(isDown)
        SystemApi::dispatchKeyDown(cb,k,evt.keyCode); else
        SystemApi::dispatchKeyUp(cb,k,evt.keyCode);
      return;
      }
    case NSEventTypeAppKitDefined:
      break;
    case NSEventTypeMouseEntered:
    case NSEventTypeMouseExited:
      break;

    default:
      break;
    }
  [NSApp sendEvent: evt];
  }
