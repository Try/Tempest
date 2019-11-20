#include "x11api.h"

#ifdef __LINUX__
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xos.h>
#include <GL/glx.h> //FIXME

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

static Event::MouseButton toButton( XButtonEvent& msg ){
  if( msg.button==Button1 )
    return Event::ButtonLeft;

  if( msg.button==Button3 )
    return Event::ButtonRight;

  if( msg.button==Button2 )
    return Event::ButtonMid;

  return Event::ButtonNone;
  }

X11Api::X11Api() {
  dpy = XOpenDisplay(nullptr);

  if(dpy == nullptr)
    throw std::runtime_error("cannot connect to X server!");

  root = DefaultRootWindow(dpy);
  }

void *X11Api::display() {
  return dpy;
  }

SystemApi::Window *X11Api::implCreateWindow(Tempest::Window *owner, uint32_t w, uint32_t h) {
  GLint att[] = { GLX_RGBA, GLX_DEPTH_SIZE, 24, GLX_DOUBLEBUFFER, None };
  XVisualInfo * vi = glXChooseVisual(dpy, 0, att);
  XSetWindowAttributes    swa;

  Colormap cmap;
  cmap = XCreateColormap(dpy, root, vi->visual, AllocNone);

  swa.override_redirect = true;
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

uint32_t X11Api::implWidth(SystemApi::Window *w) {
  XWindowAttributes xwa;
  XGetWindowAttributes(dpy, HWND(w), &xwa);

  return uint32_t(xwa.width);
  }

uint32_t X11Api::implHeight(SystemApi::Window *w) {
  XWindowAttributes xwa;
  XGetWindowAttributes(dpy, HWND(w), &xwa);

  return uint32_t(xwa.height);
  }

int X11Api::implExec(SystemApi::AppCallBack &cb) {
  // main message loop
  while (!isExit.load()) {
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

            char txt[10];
            XLookupString(&xev.xkey, txt, sizeof(txt)-1, ksym, nullptr );

            auto u16 = TextCodec::toUtf16(txt); // TODO: remove dynamic allocation
            auto key = SystemApi::translateKey(*ksym);

            uint32_t scan = xev.xkey.keycode;

            Tempest::KeyEvent e(Event::KeyType(key),uint32_t(u16.size()>0 ? u16[0] : 0),(xev.type==KeyPress) ? Event::KeyDown : Event::KeyUp);
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
        SystemApi::dispatchRender(*i.second);

        ::Window hWnd = HWND(i.first);
        ::Window root;
        int x, y;
        unsigned ww, hh, border, depth;

        // artificial move/resize event
        if( XGetGeometry(dpy, hWnd, &root, &x, &y, &ww, &hh, &border, &depth) ){
          i.second->resize(int(ww),int(hh));
          //SystemAPI::moveEvent( w, x, y );
          //SystemAPI::sizeEvent( w, ww, hh );
          }
        }
      }
    }

  return 0;
  }

#endif
