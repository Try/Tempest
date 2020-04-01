#include "x11api.h"

#ifdef __LINUX__
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xos.h>
#include <X11/keysymdef.h>
#include <X11/Xutil.h>
//#include <GL/glx.h> //FIXME

#include <Tempest/Event>
#include <Tempest/TextCodec>
#include <Tempest/Window>

#include <atomic>
#include <stdexcept>
#include <cstring>
#include <thread>
#include <unordered_map>

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
  dpy = XOpenDisplay(nullptr);

  if(dpy == nullptr)
    throw std::runtime_error("cannot connect to X server!");

  root = DefaultRootWindow(dpy);

  static const TranslateKeyPair k[] = {
    { XK_KP_Left,   Event::K_Left     },
    { XK_KP_Right,  Event::K_Right    },
    { XK_KP_Up,     Event::K_Up       },
    { XK_KP_Down,   Event::K_Down     },

    { XK_Shift_L,   Event::K_LShift   },
    { XK_Shift_R,   Event::K_RShift   },

    { XK_Control_L, Event::K_LControl },
    { XK_Control_R, Event::K_RControl },

    { XK_Escape,    Event::K_ESCAPE   },
    { XK_Tab,       Event::K_Tab      },
    { XK_BackSpace, Event::K_Back     },
    { XK_Delete,    Event::K_Delete   },
    { XK_Insert,    Event::K_Insert   },
    { XK_Home,      Event::K_Home     },
    { XK_End,       Event::K_End      },
    { XK_Pause,     Event::K_Pause    },
    { XK_Return,    Event::K_Return   },

    { XK_F1,        Event::K_F1       },
    {   48,         Event::K_0        },
    {   97,         Event::K_A        },

    { 0,            Event::K_NoKey    }
    };

  setupKeyTranslate(k,24);
  }

void *X11Api::display() {
  return dpy;
  }

void X11Api::alignGeometry(SystemApi::Window *w, Tempest::Window& owner) {
  XWindowAttributes xwa={};
  if(XGetWindowAttributes(dpy, HWND(w), &xwa)){
    Tempest::SizeEvent e(xwa.width, xwa.height);
    SystemApi::dispatchResize(owner,e);
    }
  }

SystemApi::Window *X11Api::implCreateWindow(Tempest::Window *owner, uint32_t w, uint32_t h) {
  //GLint att[] = { GLX_RGBA, GLX_DEPTH_SIZE, 24, GLX_DOUBLEBUFFER, None };
  //XVisualInfo * vi = glXChooseVisual(dpy, 0, att);

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
                   KeyPressMask | KeyReleaseMask;

  HWND win = XCreateWindow( dpy, root, 0, 0, w, h,
                            0, vi->depth, InputOutput, vi->visual,
                            CWColormap | CWEventMask, &swa );
  XSetWMProtocols(dpy, win, &wmDeleteMessage(), 1);
  XStoreName(dpy, win, wndClassName);

  XFreeColormap( dpy, cmap );
  XFree(vi);

  auto ret = reinterpret_cast<SystemApi::Window*>(win.ptr());
  windows[ret] = owner;
  //maximizeWindow(win);
  XMapWindow(dpy, win);
  XSync(dpy,False);
  if(owner!=nullptr)
    alignGeometry(win.ptr(),*owner);
  return ret;
  }

SystemApi::Window *X11Api::implCreateWindow(Tempest::Window *owner, SystemApi::ShowMode sm) {
  return implCreateWindow(owner,800,600); //TODO
  }

void X11Api::implDestroyWindow(SystemApi::Window *w) {
  windows.erase(w);
  XDestroyWindow(dpy, HWND(w));
  }

bool X11Api::implSetAsFullscreen(SystemApi::Window *w, bool fullScreen) {
  return false;
  }

bool X11Api::implIsFullscreen(SystemApi::Window *w) {
  return false;
  }

void X11Api::implSetCursorPosition(int x, int y) {
  // TODO
  }

void X11Api::implShowCursor(bool show) {
  // TODO
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
    Tempest::Window* cb = windows.find(hWnd.ptr())->second; //TODO: validation
    switch( xev.type ) {
      case ClientMessage: {
        if( xev.xclient.data.l[0] == long(wmDeleteMessage()) ){
          SystemApi::exit();
          }
        break;
        }
      case ButtonPress:
      case ButtonRelease: {
        if(cb) {
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
              ticks = 100;
              }
            else if ( xev.xbutton.button == Button5 ) {
              ticks = -100;
              }
            Tempest::MouseEvent e( xev.xbutton.x,
                                   xev.xbutton.y,
                                   Tempest::Event::ButtonNone,
                                   ticks,
                                   0,
                                   Event::MouseWheel );
            SystemApi::dispatchMouseWheel(*cb, e);
            } else {
            MouseEvent e( xev.xbutton.x,
                          xev.xbutton.y,
                          toButton( xev.xbutton ),
                          0,
                          0,
                          xev.type==ButtonPress ? Event::MouseDown : Event::MouseUp );
            if(xev.type==ButtonPress)
              SystemApi::dispatchMouseDown(*cb, e); else
              SystemApi::dispatchMouseUp(*cb, e);
            }
          }
        break;
        }
      case MotionNotify: {
        MouseEvent e( xev.xmotion.x,
                      xev.xmotion.y,
                      Event::ButtonNone,
                      0,
                      0,
                      Event::MouseMove  );
        SystemApi::dispatchMouseMove(*cb, e);
        break;
        }
      case KeyPress:
      case KeyRelease: {
        if(cb) {
          int keysyms_per_keycode_return = 0;
          KeySym *ksym = XGetKeyboardMapping( dpy, KeyCode(xev.xkey.keycode),
                                              1,
                                              &keysyms_per_keycode_return );

          char txt[10]={};
          XLookupString(&xev.xkey, txt, sizeof(txt)-1, ksym, nullptr );

          auto u16 = TextCodec::toUtf16(txt); // TODO: remove dynamic allocation
          auto key = SystemApi::translateKey(*ksym);

          uint32_t scan = xev.xkey.keycode;

          Tempest::KeyEvent e(Event::KeyType(key),uint32_t(u16.size()>0 ? u16[0] : 0),Event::M_NoModifier,(xev.type==KeyPress) ? Event::KeyDown : Event::KeyUp);
          if(xev.type==KeyPress)
            SystemApi::dispatchKeyDown(*cb,e,scan); else
            SystemApi::dispatchKeyUp  (*cb,e,scan);
            }
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
