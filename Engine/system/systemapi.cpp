#include "systemapi.h"

#include "exceptions/exception.h"
#include <Tempest/Event>
#include <thread>
#include <unordered_set>

#include <windows.h>

using namespace Tempest;

struct KeyInf {
  KeyInf(){}

  std::vector<SystemApi::TranslateKeyPair> keys;
  std::vector<SystemApi::TranslateKeyPair> a, k0, f1;
  uint16_t                                 fkeysCount=1;
  };

static LRESULT CALLBACK WindowProc(HWND hWnd,UINT msg,const WPARAM wParam,const LPARAM lParam );

static const wchar_t*                         wndClassName=L"Tempest.Window";
static std::unordered_set<SystemApi::Window*> windows;
static KeyInf                                 ki;

static int getIntParam(DWORD_PTR v){
  if(v>std::numeric_limits<int16_t>::max())
    return int(v)-std::numeric_limits<uint16_t>::max();
  return int(v);
  }

static int getX_LPARAM(LPARAM lp) {
  DWORD_PTR x = (DWORD_PTR(lp)) & 0xffff;
  return getIntParam(x);
  }

static int getY_LPARAM(LPARAM lp) {
  DWORD_PTR y = ((DWORD_PTR(lp)) >> 16) & 0xffff;
  return getIntParam(y);
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

bool SystemApi::implInitWindowClass() {
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
  return true;
  }

bool SystemApi::initWindowClass() {
  static const bool flag = implInitWindowClass();
  (void)flag;
  }

SystemApi::Window *SystemApi::createWindow(SystemApi::WindowCallback *callback, uint32_t width, uint32_t height) {
  initWindowClass();
  // Create window with the registered class:
  RECT wr = {0, 0, static_cast<LONG>(width), static_cast<LONG>(height)};
  AdjustWindowRect(&wr, WS_OVERLAPPEDWINDOW, FALSE);
  HWND window = CreateWindowExW(0,
                                wndClassName,          // class name
                                nullptr,               // app name
                                WS_OVERLAPPEDWINDOW |  // window style
                                WS_VISIBLE | WS_SYSMENU,
                                0, 0,                  // x/y coords
                                wr.right - wr.left,    // width
                                wr.bottom - wr.top,    // height
                                nullptr,               // handle to parent
                                nullptr,               // handle to menu
                                GetModuleHandle(nullptr),          // hInstance
                                nullptr);              // no extra parameters

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

SystemApi::Window *SystemApi::createWindow(SystemApi::WindowCallback *cb,ShowMode sm) {
  SystemApi::Window* hwnd = nullptr;
  if(sm==Maximized) {
    int w = GetSystemMetrics(SM_CXFULLSCREEN),
        h = GetSystemMetrics(SM_CYFULLSCREEN);
    hwnd = createWindow(cb,uint32_t(w),uint32_t(h));
    ShowWindow(HWND(hwnd),SW_MAXIMIZE);
    }
  else if(sm==Minimized) {
    hwnd =  createWindow(cb,800,600);
    ShowWindow(HWND(hwnd),SW_MINIMIZE);
    }
  else {
    hwnd =  createWindow(cb,800,600);
    ShowWindow(HWND(hwnd),SW_NORMAL);
    }
  // TODO: fullscreen
  return hwnd;
  }

void SystemApi::destroyWindow(SystemApi::Window *w) {
  windows.erase(w);
  DestroyWindow(HWND(w));
  }

uint16_t SystemApi::translateKey(uint64_t scancode) {
  for(size_t i=0; i<ki.keys.size(); ++i)
    if( ki.keys[i].src==scancode )
      return ki.keys[i].result;

  for(size_t i=0; i<ki.k0.size(); ++i)
    if( ki.k0[i].src<=scancode &&
                      scancode<=ki.k0[i].src+9 ){
      auto dx = ( scancode-ki.k0[i].src );
      return Event::KeyType( ki.k0[i].result + dx );
      }

  uint16_t literalsCount = (Event::K_Z - Event::K_A);
  for(size_t i=0; i<ki.a.size(); ++i)
    if(ki.a[i].src<=scancode &&
                    scancode<=ki.a[i].src+literalsCount ){
      auto dx = ( scancode-ki.a[i].src );
      return Event::KeyType( ki.a[i].result + dx );
      }

  for(size_t i=0; i<ki.f1.size(); ++i)
    if(ki.f1[i].src<=scancode &&
                     scancode<=ki.f1[i].src+ki.fkeysCount ){
      auto dx = (scancode-ki.f1[i].src);
      return Event::KeyType(ki.f1[i].result+dx);
      }

  return Event::K_NoKey;
  }

void SystemApi::setupKeyTranslate( const TranslateKeyPair k[] ) {
  ki.keys.clear();
  ki.a. clear();
  ki.k0.clear();
  ki.f1.clear();

  for( size_t i=0; k[i].result!=Event::K_NoKey; ++i ){
    if( k[i].result==Event::K_A )
      ki.a.push_back(k[i]); else
    if( k[i].result==Event::K_0 )
      ki.k0.push_back(k[i]); else
    if( k[i].result==Event::K_F1 )
      ki.f1.push_back(k[i]); else
      ki.keys.push_back( k[i] );
    }

#ifndef __ANDROID__
  ki.keys.shrink_to_fit();
  ki.a. shrink_to_fit();
  ki.k0.shrink_to_fit();
  ki.f1.shrink_to_fit();
#endif
  }

int SystemApi::exec(AppCallBack& cb) {
  bool done=false;
  // main message loop
  while (!done) {
    MSG msg={};
    if(PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
      if( msg.message==WM_QUIT ) { // check for a quit message
        done = true;  // if found, quit app
        } else {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
        }
      std::this_thread::yield();
      } else {
      if(cb.onTimer()==0)
        Sleep(1);
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
    case WM_MOUSEWHEEL:{
       POINT p;
       p.x = getX_LPARAM(lParam);
       p.y = getY_LPARAM(lParam);

       ScreenToClient(hWnd, &p);

       Tempest::MouseEvent e( p.x, p.y,
                              Tempest::Event::ButtonNone,
                              GET_WHEEL_DELTA_WPARAM(wParam),
                              0,
                              Event::MouseWheel );
       cb->onMouse(e);
       }

    case WM_KEYDOWN: {
       SystemApi::translateKey(wParam);
    /*
       SystemAPI::emitEvent( w,
                             makeKeyEvent(wParam),
                             makeKeyEvent(wParam, true),
                             Event::KeyDown );*/
      }
      break;

    case WM_KEYUP: {
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
