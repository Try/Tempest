#include "x11api.h"

#ifdef __LINUX__
#include <Tempest/Event>
#include <Tempest/TextCodec>
#include <Tempest/Window>

#include <atomic>
#include <stdexcept>
#include <cstring>
#include <thread>
#include <unordered_map>

#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xcursor/Xcursor.h>
#include <X11/Xatom.h>
#include <X11/Xos.h>
#include <X11/keysymdef.h>
#include <X11/Xutil.h>
#undef CursorShape

struct HWND final {
  ::Window wnd;
  HWND()=default;
  HWND(::Window w):wnd(w) {
    }
  HWND(Tempest::SystemApi::Window* p) {
    std::memcpy(&wnd,&p,sizeof(wnd));
    }

  HWND& operator = (::Window w) { wnd = w; return *this; }

  operator ::Window() { return wnd; }

  Tempest::SystemApi::Window* ptr() {
    static_assert (sizeof(::Window)<=sizeof(void*),"cannot map X11 window handle to pointer");
    Tempest::SystemApi::Window* ret=nullptr;
    std::memcpy(&ret,&wnd,sizeof(wnd));
    return ret;
    }
  };

using namespace Tempest;

static const char*      wndClassName="Tempest.Window";
static Display*         dpy  = nullptr;
static ::Window         root = {};
static std::atomic_bool isExit{0};
static int              activeCursorChange = 0;
static XIM              xim = 0;

static std::unordered_map<SystemApi::Window*,Tempest::Window*> windows;

static Atom& wmDeleteMessage(){
  static Atom w  = XInternAtom( dpy, "WM_DELETE_WINDOW", 0);
  return w;
  }

static Atom& _NET_WM_STATE(){
  static Atom w  = XInternAtom( dpy, "_NET_WM_STATE", 0);
  return w;
  }

static Atom& _NET_WM_STATE_MAXIMIZED_HORZ(){
  static Atom w  = XInternAtom( dpy, "_NET_WM_STATE_MAXIMIZED_HORZ", 0);
  return w;
  }

static Atom& _NET_WM_STATE_MAXIMIZED_VERT(){
  static Atom w  = XInternAtom( dpy, "_NET_WM_STATE_MAXIMIZED_VERT", 0);
  return w;
  }

static Atom& _NET_WM_STATE_FULLSCREEN(){
  static Atom w  = XInternAtom( dpy, "_NET_WM_STATE_FULLSCREEN", 0);
  return w;
  }

static Event::MouseButton toButton( XButtonEvent& msg ){
  if( msg.button==Button1 )
    return Event::ButtonLeft;

  if( msg.button==Button3 )
    return Event::ButtonRight;

  if( msg.button==Button2 )
    return Event::ButtonMid;

  if( msg.button==8 )
    return Event::ButtonBack;

  if( msg.button==9 )
    return Event::ButtonForward;

  return Event::ButtonNone;
  }

static void maximizeWindow(HWND& w) {
  Atom a[2];
  a[0] = _NET_WM_STATE_MAXIMIZED_HORZ();
  a[1] = _NET_WM_STATE_MAXIMIZED_VERT();

  XChangeProperty ( dpy, w, _NET_WM_STATE(),
    XA_ATOM, 32, PropModeReplace, (unsigned char*)a, 2);
  XSync(dpy,False);
  }

X11Api::X11Api() {
  static const TranslateKeyPair k[] = {
    { XK_Control_L, Event::K_LControl },
    { XK_Control_R, Event::K_RControl },

    { XK_Shift_L,   Event::K_LShift   },
    { XK_Shift_R,   Event::K_RShift   },

    { XK_Alt_L,     Event::K_LAlt     },
    { XK_Alt_R,     Event::K_RAlt     },

    { XK_Left,      Event::K_Left     },
    { XK_Right,     Event::K_Right    },
    { XK_Up,        Event::K_Up       },
    { XK_Down,      Event::K_Down     },

    { XK_Escape,    Event::K_ESCAPE   },
    { XK_Tab,       Event::K_Tab      },
    { XK_BackSpace, Event::K_Back     },
    { XK_Delete,    Event::K_Delete   },
    { XK_Insert,    Event::K_Insert   },
    { XK_Home,      Event::K_Home     },
    { XK_End,       Event::K_End      },
    { XK_Pause,     Event::K_Pause    },
    { XK_Return,    Event::K_Return   },
    { XK_space,     Event::K_Space    },
    { XK_Caps_Lock, Event::K_CapsLock },

    { XK_F1,        Event::K_F1       },
    {   48,         Event::K_0        },
    {   97,         Event::K_A        },

    { 0,            Event::K_NoKey    }
    };
  setupKeyTranslate(k,24);

  XInitThreads();
  dpy = XOpenDisplay(nullptr);

  if(dpy != nullptr)
    root = DefaultRootWindow(dpy);

  // X input method setup, only strictly necessary for intl key text
  // loads the XMODIFIERS environment variable to see what IME to use
  XSetLocaleModifiers("");

  xim = XOpenIM(dpy, 0, 0, 0);
  if(xim==nullptr){
    // fallback to internal input method
    XSetLocaleModifiers("@im=none");
    xim = XOpenIM(dpy, 0, 0, 0);
    }
  }

void* X11Api::display() {
  SystemApi::inst();
  return dpy;
  }

void X11Api::alignGeometry(SystemApi::Window *w, Tempest::Window& owner) {
  XWindowAttributes xwa={};
  if(XGetWindowAttributes(dpy, HWND(w), &xwa)) {
    owner.setPosition(xwa.x,xwa.y);
    Tempest::SizeEvent e(xwa.width, xwa.height);
    SystemApi::dispatchResize(owner,e);
    }
  }

SystemApi::Window *X11Api::implCreateWindow(Tempest::Window *owner, uint32_t w, uint32_t h) {
  if(dpy==nullptr)
    return nullptr;

  long visualMask = VisualScreenMask;
  int numberOfVisuals;
  XVisualInfo vInfoTemplate = {};
  vInfoTemplate.screen = DefaultScreen(dpy);
  XVisualInfo * vi = XGetVisualInfo(dpy, visualMask, &vInfoTemplate, &numberOfVisuals);

  Colormap cmap;
  cmap = XCreateColormap(dpy, root, vi->visual, AllocNone);

  XSetWindowAttributes swa={};
  swa.colormap   = cmap;
  swa.event_mask = PointerMotionMask | ExposureMask |
                   ButtonPressMask | ButtonReleaseMask |
                   KeyPressMask | KeyReleaseMask |
                   FocusChangeMask;

  HWND win = XCreateWindow( dpy, root, 0, 0, w, h,
                            0, vi->depth, InputOutput, vi->visual,
                            CWColormap | CWEventMask, &swa );
  XSetWMProtocols(dpy, win, &wmDeleteMessage(), 1);
  XStoreName(dpy, win, wndClassName);

  XFreeColormap( dpy, cmap );
  XFree(vi);

  auto ret = reinterpret_cast<SystemApi::Window*>(win.ptr());
  windows[ret] = owner;
  XMapWindow(dpy, win);
  XSync(dpy,False);

  // X input context, you can have multiple for text boxes etc, but having a
  // single one is the easiest.
  XIC xic = XCreateIC(xim,
                      XNInputStyle,   XIMPreeditNothing | XIMStatusNothing,
                      XNClientWindow, win,
                      XNFocusWindow,  win,
                      NULL);
  XSetICFocus(xic);

  if(owner!=nullptr)
    alignGeometry(win.ptr(),*owner);
  return ret;
  }

SystemApi::Window *X11Api::implCreateWindow(Tempest::Window *owner, SystemApi::ShowMode sm) {
  if(dpy==nullptr)
    return nullptr;

  Screen* s = DefaultScreenOfDisplay(dpy);
  int width = s->width;
  int height = s->height;
  
  SystemApi::Window* hwnd = nullptr;

  switch(sm) {
    case Hidden:
      hwnd = implCreateWindow(owner,1, 1);
      // TODO: make window invisible
      break;
    case Normal:
      hwnd = implCreateWindow(owner,800, 600);
      break;
    case Minimized:
    case Maximized:
      // TODO
      hwnd = implCreateWindow(owner,width, height);
      break;
    case FullScreen:
      hwnd = implCreateWindow(owner,width, height);
      implSetAsFullscreen(hwnd,true);
      break;
    }
  
  return hwnd;
  }

void X11Api::implDestroyWindow(SystemApi::Window *w) {
  windows.erase(w); //NOTE: X11 can send events to ded window
  XDestroyWindow(dpy, HWND(w));
  }

bool X11Api::implSetAsFullscreen(SystemApi::Window *w, bool fullScreen) {
  XEvent e = {};
  e.xclient.type         = ClientMessage;
  e.xclient.window       = HWND(w);
  e.xclient.message_type = _NET_WM_STATE();
  e.xclient.format       = 32;
  e.xclient.data.l[0]    = 2;    // _NET_WM_STATE_TOGGLE
  e.xclient.data.l[1]    = _NET_WM_STATE_FULLSCREEN();
  e.xclient.data.l[2]    = 0;    // no second property to toggle
  
  if(fullScreen)
    e.xclient.data.l[3]  = 1;
  else
    e.xclient.data.l[3]  = 0;

  XSendEvent(dpy, DefaultRootWindow(dpy), False, SubstructureRedirectMask | SubstructureNotifyMask, &e);
  return true;
  }

bool X11Api::implIsFullscreen(SystemApi::Window *w) {
  Atom actualType;
  int actualFormat;
  unsigned long numItems, bytesAfter;
  unsigned char *propertyValue = NULL;
  long maxLength = 1024;

  if(XGetWindowProperty(dpy, HWND(w), _NET_WM_STATE(),
                        0l, maxLength, False, XA_ATOM, &actualType,
                        &actualFormat, &numItems, &bytesAfter,
                        &propertyValue) == Success) {
    int fullscreen = 0;
    Atom *atoms = reinterpret_cast<Atom*>(propertyValue);

    for(uint32_t i = 0; i < numItems; ++i) {
      if(atoms[i] == _NET_WM_STATE_FULLSCREEN()) {
        fullscreen = 1;
        }
      }

    if(fullscreen)
      return true;
    }
  return false;
  }

void X11Api::implSetCursorPosition(SystemApi::Window *w, int x, int y) {
  activeCursorChange = 1;
  XWarpPointer(dpy, None, HWND(w), 0, 0, 0, 0, x, y);
  XFlush(dpy);
  }

void X11Api::implShowCursor(SystemApi::Window *w, CursorShape show) {
  HWND hwnd(w);

  if(show==CursorShape::Hidden) {
    Cursor invisibleCursor;
    ::Pixmap bitmapNoData;
    XColor black;
    static char noData[] = { 0,0,0,0,0,0,0,0 };
    black.red = black.green = black.blue = 0;

    bitmapNoData    = XCreateBitmapFromData(dpy, hwnd, noData, 8, 8);
    invisibleCursor = XCreatePixmapCursor(dpy, bitmapNoData, bitmapNoData,
                                          &black, &black, 0, 0);
    XDefineCursor(dpy, hwnd, invisibleCursor);
    XFreeCursor(dpy, invisibleCursor);
    XFreePixmap(dpy, bitmapNoData);
    } else {
    const char* name = nullptr;
    switch(show) {
      case CursorShape::Arrow:
      case CursorShape::Hidden:
        break;
      case CursorShape::IBeam:
        name = "xterm";
        break;
      case CursorShape::SizeHor:
        name = "size_hor";
        break;
      case CursorShape::SizeVer:
        name = "size_ver";
        break;
      case CursorShape::SizeFDiag:
        name = "size_fdiag";
        break;
      case CursorShape::SizeBDiag:
        name = "size_bdiag";
        break;
      case CursorShape::SizeAll:
        name = "size_all";
        break;
      }
    Cursor c = name==nullptr ? 0 : XcursorLibraryLoadCursor(dpy,name);
    if(c==0) {
      XUndefineCursor(dpy, hwnd);
      } else {
      XDefineCursor(dpy, hwnd, c);
      XFreeCursor(dpy, c);
      }
    }

  XSync(dpy, False);
  }

Rect X11Api::implWindowClientRect(SystemApi::Window *w) {
  XWindowAttributes xwa;
  XGetWindowAttributes(dpy, HWND(w), &xwa);

  return Rect( xwa.x, xwa.y, xwa.width, xwa.height );
  }

void X11Api::implExit() {
  isExit.store(true);
  }

bool X11Api::implIsRunning() {
  return !isExit.load();
  }

int X11Api::implExec(SystemApi::AppCallBack &cb) {
  // main message loop
  while (!isExit.load()) {
    implProcessEvents(cb);
    }
  return 0;
  }

void X11Api::implProcessEvents(SystemApi::AppCallBack &cb) {
  // main message loop
  if(XPending(dpy)>0) {
    XEvent xev={};
    XNextEvent(dpy, &xev);

    HWND hWnd = xev.xclient.window;
    auto it = windows.find(hWnd.ptr());
    if(it==windows.end() || it->second==nullptr)
      return;
    Tempest::Window& cb = *it->second; //TODO: validation
    switch( xev.type ) {
      case ClientMessage: {
        if( xev.xclient.data.l[0] == long(wmDeleteMessage()) ){
          SystemApi::exit();
          }
        break;
        }
      case MappingNotify:
        XRefreshKeyboardMapping(&xev.xmapping);
        break;
      case ButtonPress:
      case ButtonRelease: {
        bool isWheel = false;
        if( xev.type==ButtonPress && XPending(dpy) &&
            (xev.xbutton.button == Button4 || xev.xbutton.button == Button5) ){
          XEvent ev;
          XNextEvent(dpy, &ev);
          isWheel = (ev.type==ButtonRelease);
          }

        if( isWheel ){
          int ticks = 0;
          if( xev.xbutton.button == Button4 ) {
            ticks = 120;
            }
          else if ( xev.xbutton.button == Button5 ) {
            ticks = -120;
            }
          Tempest::MouseEvent e( xev.xbutton.x,
                                 xev.xbutton.y,
                                 Tempest::Event::ButtonNone,
                                 Event::M_NoModifier,
                                 ticks,
                                 0,
                                 Event::MouseWheel );
          SystemApi::dispatchMouseWheel(cb, e);
          } else {
          MouseEvent e( xev.xbutton.x,
                        xev.xbutton.y,
                        toButton( xev.xbutton ),
                        Event::M_NoModifier,
                        0,
                        0,
                        xev.type==ButtonPress ? Event::MouseDown : Event::MouseUp );
          if(xev.type==ButtonPress)
            SystemApi::dispatchMouseDown(cb, e); else
            SystemApi::dispatchMouseUp(cb, e);
          }
        break;
        }
      case MotionNotify: {
        if(activeCursorChange == 1) {
          // FIXME: mouse behave crazy in OpenGothic
          activeCursorChange = 0;
          break;
        }
        MouseEvent e( xev.xmotion.x,
                      xev.xmotion.y,
                      Event::ButtonNone,
                      Event::M_NoModifier,
                      0,
                      0,
                      Event::MouseMove  );
        SystemApi::dispatchMouseMove(cb, e);
        break;
        }
      case KeyPress:
      case KeyRelease: {
        KeySym ksym = XLookupKeysym(&xev.xkey,0);

        // NOTE: it's not optimal to recreate xic on every button, but it's good engough
        XIC xic = XCreateIC(xim,
                            XNInputStyle,   XIMPreeditNothing | XIMStatusNothing,
                            XNClientWindow, xev.xclient.window,
                            XNFocusWindow,  xev.xclient.window,
                            nullptr);
        char txt[64]={};
        if(xic!=nullptr) {
          XEvent xev2 = xev;
          xev2.type = KeyPress; // HACK: Their behavior when a client passes a KeyRelease event is undefined.
          Status status = {};
          Xutf8LookupString(xic, &xev2.xkey, txt, sizeof(txt)-1, &ksym, &status);
          if(status == XBufferOverflow) {
            txt[0] = '\0';
            }
          // XLookupString(&xev.xkey, txt, sizeof(txt)-1, ksym, nullptr );
          XDestroyIC(xic);
          }

        auto u16 = TextCodec::toUtf16(txt); // TODO: remove dynamic allocation
        auto key = SystemApi::translateKey(ksym);

        uint32_t scan = xev.xkey.keycode;
        // printf("%s\n",txt);

        Tempest::KeyEvent e(Event::KeyType(key),uint32_t(u16.size()>0 ? u16[0] : 0),Event::M_NoModifier,(xev.type==KeyPress) ? Event::KeyDown : Event::KeyUp);
        if(xev.type==KeyPress)
          SystemApi::dispatchKeyDown(cb,e,scan); else
          SystemApi::dispatchKeyUp  (cb,e,scan);
        break;
        }
      case FocusIn: {
        FocusEvent e(true, Event::UnknownReason);
        SystemApi::dispatchFocus(cb, e);
        break;
      }
      case FocusOut: {
        FocusEvent e(false, Event::UnknownReason);
        SystemApi::dispatchFocus(cb, e);
        break;
      }
      }

    std::this_thread::yield();
    } else {
    if(cb.onTimer()==0)
      std::this_thread::yield();
    for(auto& i:windows) {
      if(i.second==nullptr)
        continue;
      // artificial move/resize event
      alignGeometry(i.first,*i.second);
      SystemApi::dispatchRender(*i.second);
      }
    }
  }

#endif
