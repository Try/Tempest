#include "systemapi.h"

#include <thread>
#include <windows.h>
#include <unordered_set>
#include "exceptions/exception.h"

#include <Tempest/Event>

using namespace Tempest;

static LRESULT CALLBACK WindowProc(HWND hWnd,UINT msg,const WPARAM wParam,const LPARAM lParam );
static const wchar_t* wndClassName=L"Tempest.Window";
static std::unordered_set<SystemApi::Window*> windows;

static int getX_LPARAM(LPARAM lp) {
  return (DWORD_PTR(lp)) & 0xffff;
  }

static int getY_LPARAM(LPARAM lp) {
  return ((DWORD_PTR(lp)) >> 16) & 0xffff;
  }

static WORD get_Button_WPARAM(WPARAM lp) {
  return ((DWORD_PTR(lp)) >> 16) & 0xffff;
  }

static Event::MouseButton toButton( UINT msg, DWORD wParam ){
  if( msg==WM_LBUTTONDOWN ||
      msg==WM_LBUTTONUP )
    return Event::ButtonLeft;

  if( msg==WM_RBUTTONDOWN  ||
      msg==WM_RBUTTONUP)
    return Event::ButtonRight;

  if( msg==WM_MBUTTONDOWN ||
      msg==WM_MBUTTONUP )
    return Event::ButtonMid;

  if( msg==WM_XBUTTONDOWN ||
      msg==WM_XBUTTONUP ) {
    const WORD btn = get_Button_WPARAM(wParam);
    if(btn==XBUTTON1)
      return Event::ButtonBack;
    if(btn==XBUTTON2)
      return Event::ButtonForward;
    }

  return Event::ButtonNone;
  }

SystemApi::SystemApi() {}

SystemApi::Window *SystemApi::createWindow(SystemApi::WindowCallback *callback, uint32_t width, uint32_t height) {
  WNDCLASSEXW winClass={};

  // Initialize the window class structure:
  winClass.lpszClassName = wndClassName;
  winClass.cbSize        = sizeof(WNDCLASSEX);
  winClass.style         = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
  winClass.lpfnWndProc   = WindowProc;
  winClass.hInstance     = GetModuleHandle(nullptr);
  winClass.hIcon         = LoadIcon( GetModuleHandle(nullptr), LPCTSTR(MAKEINTRESOURCE(32512)) );
  winClass.hIconSm       = LoadIcon( GetModuleHandle(nullptr), LPCTSTR(MAKEINTRESOURCE(32512)) );
  winClass.hCursor       = LoadCursor(nullptr, IDC_ARROW);
  winClass.hbrBackground = nullptr;// (HBRUSH)GetStockObject(BLACK_BRUSH);
  winClass.lpszMenuName  = nullptr;
  winClass.cbClsExtra    = 0;
  winClass.cbWndExtra    = 0;

  // Register window class:
  if(!RegisterClassExW(&winClass)) {
    throw std::system_error(Tempest::SystemErrc::InvalidWindowClass);
    }

  // Create window with the registered class:
  RECT wr = {0, 0, static_cast<LONG>(width), static_cast<LONG>(height)};
  AdjustWindowRect(&wr, WS_OVERLAPPEDWINDOW, FALSE);
  HWND window = CreateWindowExW(0,
                                wndClassName,          // class name
                                nullptr,               // app name
                                WS_OVERLAPPEDWINDOW |  // window style
                                WS_VISIBLE | WS_SYSMENU,
                                100, 100,            // x/y coords
                                wr.right - wr.left,  // width
                                wr.bottom - wr.top,  // height
                                nullptr,             // handle to parent
                                nullptr,             // handle to menu
                                GetModuleHandle(nullptr),          // hInstance
                                nullptr);            // no extra parameters

  if( !window )
    throw std::system_error(Tempest::SystemErrc::UnableToCreateWindow);

  SetWindowLongPtr(window,GWLP_USERDATA,LONG_PTR(callback));
  //minsize.x = GetSystemMetrics(SM_CXMINTRACK);
  //minsize.y = GetSystemMetrics(SM_CYMINTRACK) + 1;
  Window* wx = reinterpret_cast<Window*>(window);
  try {
    windows.insert(wx);
    }
  catch(...){
    destroyWindow(wx);
    throw;
    }
  return wx;
  }

void SystemApi::destroyWindow(SystemApi::Window *w) {
  windows.erase(w);
  DestroyWindow(HWND(w));
  }

int SystemApi::exec() {
  bool done=false;
  // main message loop
  while (!done) {
    MSG  msg={};
    if(PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
      if( msg.message==WM_QUIT ) { // check for a quit message
        done = true;  // if found, quit app
        } else {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
        }
      std::this_thread::yield();
      } else {
      for(auto& i:windows){
        HWND h = HWND(i);
        SystemApi::WindowCallback* cb=reinterpret_cast<SystemApi::WindowCallback*>(GetWindowLongPtr(h,GWLP_USERDATA));
        if(cb)
          cb->onRender(reinterpret_cast<SystemApi::Window*>(h));
        }
      }
    }
  return 0;
  }

uint32_t SystemApi::width(SystemApi::Window *w) {
  RECT rect={};
  GetClientRect(HWND(w),&rect);
  return uint32_t(rect.right-rect.left);
  }

uint32_t SystemApi::height(SystemApi::Window *w) {
  RECT rect={};
  GetClientRect(HWND(w),&rect);
  return uint32_t(rect.bottom-rect.top);
  }

Rect SystemApi::windowClientRect(SystemApi::Window* w) {
  RECT rectWindow;
  GetClientRect( HWND(w), &rectWindow);
  int cW = rectWindow.right  - rectWindow.left;
  int cH = rectWindow.bottom - rectWindow.top;

  return Rect(rectWindow.left,rectWindow.top,cW,cH);
  }

LRESULT CALLBACK WindowProc( HWND   hWnd,
                             UINT   msg,
                             const WPARAM wParam,
                             const LPARAM lParam ) {
  SystemApi::WindowCallback* cb=reinterpret_cast<SystemApi::WindowCallback*>(GetWindowLongPtr(hWnd,GWLP_USERDATA));
  switch( msg ) {
    case WM_PAINT:{
      cb->onRender(reinterpret_cast<SystemApi::Window*>(hWnd));
      }
      break;
    case WM_CLOSE:{
      //Tempest::CloseEvent e;
      //SystemAPI::emitEvent(w, e);
      //if( !e.isAccepted() )
        PostQuitMessage(0);
      }
      break;

    case WM_XBUTTONDOWN:
    case WM_LBUTTONDOWN:
    case WM_MBUTTONDOWN:
    case WM_RBUTTONDOWN: {
      SetCapture(hWnd);
      MouseEvent e( getX_LPARAM(lParam),
                    getY_LPARAM(lParam),
                    toButton(msg,wParam),
                    0,
                    0,
                    Event::MouseDown );
      if(cb)
        cb->onMouse(e);
      }
      break;

    case WM_XBUTTONUP:
    case WM_LBUTTONUP:
    case WM_RBUTTONUP:
    case WM_MBUTTONUP: {
      ReleaseCapture();
      MouseEvent e( getX_LPARAM(lParam),
                    getY_LPARAM(lParam),
                    toButton(msg,wParam),
                    0,
                    0,
                    Event::MouseUp  );
      if(cb)
        cb->onMouse(e);
      }
      break;

    case WM_MOUSEMOVE: {
      if(cb) {
        MouseEvent e0( getX_LPARAM(lParam),
                       getY_LPARAM(lParam),
                       Event::ButtonNone,
                       0,
                       0,
                       Event::MouseDrag  );
        cb->onMouse(e0);
        if(!e0.isAccepted()) {
          MouseEvent e1( getX_LPARAM(lParam),
                         getY_LPARAM(lParam),
                         Event::ButtonNone,
                         0,
                         0,
                         Event::MouseMove  );
          cb->onMouse(e1);
          }
        }
      }
      break;

    case WM_SIZE:
      if(wParam!=SIZE_MINIMIZED) {
        RECT rpos = {0,0,0,0};
        GetWindowRect( hWnd, &rpos );

        RECT rectWindow;
        GetClientRect( HWND(hWnd), &rectWindow);
        int cW = rectWindow.right  - rectWindow.left;
        int cH = rectWindow.bottom - rectWindow.top;

        int width  = lParam & 0xffff;
        int height = (uint32_t(lParam) & 0xffff0000) >> 16;
        if(cb)
          cb->onResize(reinterpret_cast<SystemApi::Window*>(hWnd),width,height);
        }
      break;

    case WM_DESTROY: {
      PostQuitMessage(0);
      }
      break;
    //default:
      //if(cb)
      //  cb->onRender(reinterpret_cast<SystemApi::Window*>(hWnd));
    }
  return DefWindowProc( hWnd, msg, wParam, lParam );
  }
