#include "windowsapi.h"

#ifdef __WINDOWS__

#include <Tempest/Event>
#include <Tempest/Except>

#include "system/eventdispatcher.h"

#include <atomic>
#include <unordered_set>
#include <thread>
#include <vector>

#include <windows.h>

using namespace Tempest;

static const wchar_t*                         wndClassName=L"Tempest.Window";
static std::unordered_set<SystemApi::Window*> windows;
static std::atomic_bool                       isExit{0};

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

static Event::MouseButton toButton( UINT msg, const unsigned long long wParam ){
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

WindowsApi::WindowsApi() {
  static const TranslateKeyPair k[] = {
    { VK_LCONTROL, Event::K_LControl },
    { VK_RCONTROL, Event::K_RControl },

    { VK_LSHIFT,   Event::K_LShift   },
    { VK_RSHIFT,   Event::K_RShift   },

    { VK_MENU,     Event::K_LAlt     },

    { VK_LEFT,     Event::K_Left     },
    { VK_RIGHT,    Event::K_Right    },
    { VK_UP,       Event::K_Up       },
    { VK_DOWN,     Event::K_Down     },

    { VK_ESCAPE,   Event::K_ESCAPE   },
    { VK_BACK,     Event::K_Back     },
    { VK_TAB,      Event::K_Tab      },
    { VK_DELETE,   Event::K_Delete   },
    { VK_INSERT,   Event::K_Insert   },
    { VK_HOME,     Event::K_Home     },
    { VK_END,      Event::K_End      },
    { VK_PAUSE,    Event::K_Pause    },
    { VK_RETURN,   Event::K_Return   },
    { VK_SPACE,    Event::K_Space    },
    { VK_CAPITAL,  Event::K_CapsLock },

    { VK_F1,       Event::K_F1       },
    { 0x30,        Event::K_0        },
    { 0x41,        Event::K_A        },

    { 0,           Event::K_NoKey    }
    };

  setupKeyTranslate(k,24);

  struct Private {
    static LRESULT CALLBACK wProc(HWND hWnd,UINT msg,const WPARAM wParam,const LPARAM lParam ){
      return WindowsApi::windowProc(hWnd,msg,wParam,lParam);
      }
    };

  WNDCLASSEXW winClass={};
  // Initialize the window class structure:
  winClass.lpszClassName = wndClassName;
  winClass.cbSize        = sizeof(WNDCLASSEX);
  winClass.style         = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
  winClass.lpfnWndProc   = Private::wProc;
  winClass.hInstance     = GetModuleHandle(nullptr);
  winClass.hIcon         = LoadIcon( GetModuleHandle(nullptr), LPCTSTR(MAKEINTRESOURCE(32512)) );
  winClass.hIconSm       = LoadIcon( GetModuleHandle(nullptr), LPCTSTR(MAKEINTRESOURCE(32512)) );
  winClass.hCursor       = LoadCursor(nullptr, IDC_ARROW);
  winClass.hbrBackground = nullptr;// (HBRUSH)GetStockObject(BLACK_BRUSH);
  winClass.lpszMenuName  = nullptr;
  winClass.cbClsExtra    = 0;
  winClass.cbWndExtra    = 0;

  if(RegisterClassExW(&winClass)==0)
    throw std::system_error(Tempest::SystemErrc::InvalidWindowClass);
  }

SystemApi::Window *WindowsApi::implCreateWindow(Tempest::Window *owner, uint32_t width, uint32_t height, ShowMode sm) {
  RECT wr = {0, 0, static_cast<LONG>(width), static_cast<LONG>(height)};
  AdjustWindowRect(&wr, WS_OVERLAPPEDWINDOW, FALSE);

  HINSTANCE module = GetModuleHandle(nullptr);
  DWORD     style  = 0;
  if(sm!=Hidden)
    style = WS_OVERLAPPEDWINDOW | WS_VISIBLE | WS_SYSMENU;
  HWND window = CreateWindowExW(0,
                                wndClassName,          // class name
                                nullptr,               // app name
                                style,                 // window style
                                0, 0,                  // x/y coords
                                wr.right  - wr.left,   // width
                                wr.bottom - wr.top,    // height
                                nullptr,               // handle to parent
                                nullptr,               // handle to menu
                                module,                // hInstance
                                nullptr);              // no extra parameters

  if( !window ) {
    return nullptr;
    }

  SetWindowLongPtr(window,GWLP_USERDATA,LONG_PTR(owner));
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

SystemApi::Window *WindowsApi::implCreateWindow(Tempest::Window *owner, uint32_t width, uint32_t height) {
  return implCreateWindow(owner,width,height,Normal);
  }

SystemApi::Window *WindowsApi::implCreateWindow(Tempest::Window *owner, ShowMode sm) {
  SystemApi::Window* hwnd = nullptr;
  if(sm==Hidden) {
    hwnd = implCreateWindow(owner,uint32_t(1),uint32_t(1));
    // ShowWindow(HWND(hwnd),SW_HIDE);
    }
  else if(sm==Minimized) {
    hwnd =  implCreateWindow(owner,800,600);
    ShowWindow(HWND(hwnd),SW_MINIMIZE);
    }
  else if(sm==Normal) {
    hwnd =  implCreateWindow(owner,800,600);
    ShowWindow(HWND(hwnd),SW_NORMAL);
    }
  else {
    int w = GetSystemMetrics(SM_CXFULLSCREEN),
        h = GetSystemMetrics(SM_CYFULLSCREEN);
    hwnd = implCreateWindow(owner,uint32_t(w),uint32_t(h));
    ShowWindow(HWND(hwnd),SW_MAXIMIZE);
    }
  if(sm==FullScreen)
    setAsFullscreen(hwnd,true);
  return hwnd;
  }

void WindowsApi::implDestroyWindow(SystemApi::Window *w) {
  windows.erase(w);
  DestroyWindow(HWND(w));
  }

void WindowsApi::implExit() {
  isExit.store(true);
  PostQuitMessage(0);
  }

bool WindowsApi::implIsRunning() {
  return !isExit.load();
  }

int WindowsApi::implExec(AppCallBack& cb) {
  // main message loop
  while (!isExit.load()) {
    implProcessEvents(cb);
    }
  return 0;
  }

void WindowsApi::implProcessEvents(SystemApi::AppCallBack& cb) {
  if(isExit.load())
    return;
  MSG msg={};
  if(PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE)) {
    if( msg.message==WM_QUIT ) { // check for a quit message
      isExit.store(true);  // if found, quit app
      } else {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
      }
    std::this_thread::yield();
    } else {
    if(cb.onTimer()==0)
      std::this_thread::yield();
    for(auto& i:windows){
      HWND             h  = HWND(i);
      Tempest::Window* cb = reinterpret_cast<Tempest::Window*>(GetWindowLongPtr(h,GWLP_USERDATA));
      LONG             s  = GetWindowLongA(h,GWL_STYLE);
      if(cb && 0==(s&WS_MINIMIZE))
        SystemApi::dispatchRender(*cb);
      }
    }
  }

Rect WindowsApi::implWindowClientRect(SystemApi::Window* w) {
  RECT rectWindow;
  GetClientRect( HWND(w), &rectWindow);
  int cW = rectWindow.right  - rectWindow.left;
  int cH = rectWindow.bottom - rectWindow.top;

  return Rect(rectWindow.left,rectWindow.top,cW,cH);
  }

bool WindowsApi::implSetAsFullscreen(SystemApi::Window *wx,bool fullScreen) {
  HWND hwnd = HWND(wx);
  if(fullScreen==isFullscreen(wx))
    return true;

  const int w = GetSystemMetrics(SM_CXSCREEN);
  const int h = GetSystemMetrics(SM_CYSCREEN);
  if(fullScreen) {
    //show full-screen
    SetWindowLongPtr(hwnd, GWL_STYLE, WS_VISIBLE | WS_POPUP);
    SetWindowPos    (hwnd, HWND_TOP, 0, 0, w, h, SWP_FRAMECHANGED);
    ShowWindow      (hwnd, SW_SHOW);
    } else {
    SetWindowLongPtr(hwnd, GWL_STYLE, WS_VISIBLE | WS_OVERLAPPEDWINDOW);
    SetWindowPos    (hwnd, nullptr, 0, 0, w, h, SWP_FRAMECHANGED);
    ShowWindow      (hwnd, SW_MAXIMIZE);
    }
  return true;
  }

bool WindowsApi::implIsFullscreen(SystemApi::Window *w) {
  HWND hwnd = HWND(w);
  return ULONG(GetWindowLongPtr(hwnd, GWL_STYLE)) & WS_POPUP;
  }

void WindowsApi::implSetCursorPosition(SystemApi::Window *w, int x, int y) {
  HWND  hwnd = HWND(w);
  POINT pt   = {x,y};
  ClientToScreen(hwnd, &pt);
  SetCursorPos(pt.x, pt.y);
  }

void WindowsApi::implShowCursor(bool show) {
  ShowCursor(show ? TRUE : FALSE);
  }

long long WindowsApi::windowProc(void *_hWnd, uint32_t msg, const unsigned long long wParam, const long long lParam) {
  HWND hWnd = HWND(_hWnd);

  Tempest::Window* cb=reinterpret_cast<Tempest::Window*>(GetWindowLongPtr(hWnd,GWLP_USERDATA));
  if(cb==nullptr)
    return DefWindowProcW( hWnd, msg, wParam, lParam );

  switch( msg ) {
    case WM_PAINT:{
      if(cb)
        SystemApi::dispatchRender(*cb);
      return DefWindowProc( hWnd, msg, wParam, lParam );
      }

    case WM_CLOSE:{
      CloseEvent e;
      SystemApi::dispatchClose(*cb,e);
      if(!e.isAccepted())
        exit();
      return 0;
      }

    case WM_DESTROY: {
      break;
      }

    case WM_XBUTTONDOWN:
    case WM_LBUTTONDOWN:
    case WM_MBUTTONDOWN:
    case WM_RBUTTONDOWN: {
      SetCapture(hWnd);
      if(cb) {
        MouseEvent e( getX_LPARAM(lParam),
                      getY_LPARAM(lParam),
                      toButton(msg,wParam),
                      0,
                      0,
                      Event::MouseDown );
        SystemApi::dispatchMouseDown(*cb,e);
        }
      break;
      }

    case WM_XBUTTONUP:
    case WM_LBUTTONUP:
    case WM_RBUTTONUP:
    case WM_MBUTTONUP: {
      ReleaseCapture();
      if(cb) {
        MouseEvent e( getX_LPARAM(lParam),
                      getY_LPARAM(lParam),
                      toButton(msg,wParam),
                      0,
                      0,
                      Event::MouseUp  );
        SystemApi::dispatchMouseUp(*cb,e);
        }
      break;
      }

    case WM_MOUSEMOVE: {
      if(cb) {
        MouseEvent e0( getX_LPARAM(lParam),
                       getY_LPARAM(lParam),
                       Event::ButtonNone,
                       0,
                       0,
                       Event::MouseDrag  );
        SystemApi::dispatchMouseMove(*cb,e0);
        }
      break;
      }
    case WM_MOUSEWHEEL:{
      if(cb) {
        POINT p;
        p.x = getX_LPARAM(lParam);
        p.y = getY_LPARAM(lParam);

        ScreenToClient(hWnd, &p);
        Tempest::MouseEvent e( p.x, p.y,
                               Tempest::Event::ButtonNone,
                               GET_WHEEL_DELTA_WPARAM(wParam),
                               0,
                               Event::MouseWheel );
        SystemApi::dispatchMouseWheel(*cb,e);
        }
      break;
      }
    case WM_SYSKEYDOWN:
    case WM_KEYDOWN:
    case WM_SYSKEYUP:
    case WM_KEYUP: {
      if(cb) {
        unsigned long long vkCode;
        if(wParam == VK_SHIFT) {
          uint32_t scancode = (lParam & 0x00ff0000) >> 16;
          vkCode = MapVirtualKey(scancode, MAPVK_VSC_TO_VK_EX);
          }
        else if(wParam == VK_CONTROL) {
          bool extended = (lParam & 0x01000000) != 0;
          vkCode = extended ? VK_RCONTROL : VK_LCONTROL;
          }
        else {
          vkCode = wParam;
          }

        auto key = WindowsApi::translateKey(vkCode);

        BYTE kboard[256]={};
        GetKeyboardState(kboard);
        WCHAR buf[2]={};
        if(vkCode!=VK_ESCAPE)
          ToUnicode(uint32_t(vkCode),0,kboard,buf,2,0);

        uint32_t scan = MapVirtualKeyW(uint32_t(wParam),MAPVK_VK_TO_VSC);

        Tempest::KeyEvent e(Event::KeyType(key),
                            uint32_t(buf[0]),
                            Event::M_NoModifier,
                            (msg==WM_KEYDOWN || msg==WM_SYSKEYDOWN) ? Event::KeyDown : Event::KeyUp);
        if(msg==WM_KEYDOWN || msg==WM_SYSKEYDOWN)
          SystemApi::dispatchKeyDown(*cb,e,scan); else
          SystemApi::dispatchKeyUp  (*cb,e,scan);
        }
      if(msg==WM_SYSKEYDOWN || msg==WM_SYSKEYUP)
        return DefWindowProcW( hWnd, msg, wParam, lParam );
      break;
      }

    case WM_SIZE:
      if(wParam!=SIZE_MINIMIZED) {
        int width  = lParam & 0xffff;
        int height = (uint32_t(lParam) & 0xffff0000) >> 16;
        if(cb) {
          Tempest::SizeEvent e(width,height);
          SystemApi::dispatchResize(*cb,e);
          }
        }
      break;
    default:
      return DefWindowProcW( hWnd, msg, wParam, lParam );
    }
  return 0;
  }

#endif
