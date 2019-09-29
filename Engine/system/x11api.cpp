#include "x11api.h"

#ifdef __LINUX__
#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xos.h>
#include <GL/glx.h> //FIXME

#include <atomic>
#include <stdexcept>

typedef Window HWND;

using namespace Tempest;

static const char*      wndClassName="Tempest.Window";
static Display*         dpy  = nullptr;
static HWND             root = 0;
static std::atomic_bool isExit{0};

static HWND pin( SystemApi::Window* w ){
  return *reinterpret_cast<HWND*>(w);
  }

static Atom& wmDeleteMessage(){
  static Atom w  = XInternAtom( dpy, "WM_DELETE_WINDOW", 0);
  return w;
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

SystemApi::Window *X11Api::implCreateWindow(SystemApi::WindowCallback *cb, uint32_t w, uint32_t h) {
  HWND * win = new HWND;
  GLint att[] = { GLX_RGBA, GLX_DEPTH_SIZE, 24, GLX_DOUBLEBUFFER, None };
  XVisualInfo * vi = glXChooseVisual(dpy, 0, att);
  XSetWindowAttributes    swa;

  Colormap cmap;
  cmap = XCreateColormap(dpy, root, vi->visual, AllocNone);

  swa.override_redirect = true;
  swa.colormap   = cmap;
  swa.event_mask = PointerMotionMask | ExposureMask |
      ButtonPressMask |ButtonReleaseMask|
      KeyPressMask | KeyReleaseMask;

  *win = XCreateWindow( dpy, root, 0, 0, w, h,
                        0, vi->depth, InputOutput, vi->visual,
                        CWColormap | CWEventMask, &swa );
  XSetWMProtocols(dpy, *win, &wmDeleteMessage(), 1);
  XStoreName(dpy, *win, wndClassName);

  XFreeColormap( dpy, cmap );
  XFree(vi);

  return reinterpret_cast<SystemApi::Window*>(win);
  }

SystemApi::Window *X11Api::implCreateWindow(SystemApi::WindowCallback *cb, SystemApi::ShowMode sm) {
  return implCreateWindow(cb,800,600); //TODO
  }

void X11Api::implDestroyWindow(SystemApi::Window *w) {
  XDestroyWindow(dpy, pin(w));
  //wndWx.erase( pin(w) );
  delete reinterpret_cast<HWND*>(w);
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
  XGetWindowAttributes(dpy, pin(w), &xwa);

  return Rect( xwa.x, xwa.y, xwa.width, xwa.height );
  }

void X11Api::implExit() {
  isExit.store(true);
  }

uint32_t X11Api::implWidth(SystemApi::Window *w) {
  XWindowAttributes xwa;
  XGetWindowAttributes(dpy, pin(w), &xwa);

  return uint32_t(xwa.width);
  }

uint32_t X11Api::implHeight(SystemApi::Window *w) {
  XWindowAttributes xwa;
  XGetWindowAttributes(dpy, pin(w), &xwa);

  return uint32_t(xwa.height);
  }

int X11Api::implExec(SystemApi::AppCallBack &cb) {
  // main message loop
  while (!isExit.load()) {
    XEvent xev={};
    if(!XPending(dpy)) {
      XNextEvent(dpy, &xev);
      /*
      if( msg.message==WM_QUIT ) { // check for a quit message
        isExit.store(true);  // if found, quit app
        } else {
        //xProc( xev, xev.xclient.window, quit);
        }
      std::this_thread::yield();
      } else {
      if(cb.onTimer()==0)
        usleep(1000);
      for(auto& i:windows){
        HWND h = HWND(i);
        SystemApi::WindowCallback* cb=reinterpret_cast<SystemApi::WindowCallback*>(GetWindowLongPtr(h,GWLP_USERDATA));
        if(cb)
          cb->onRender(reinterpret_cast<SystemApi::Window*>(h));
        }*/
      }
    }

  return 0;
  }

#endif
