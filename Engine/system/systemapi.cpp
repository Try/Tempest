#include "systemapi.h"

#include <windows.h>
#include "exceptions/exception.h"

using namespace Tempest;

static LRESULT CALLBACK WindowProc(HWND hWnd,UINT msg,const WPARAM wParam,const LPARAM lParam );
static const wchar_t* wndClassName=L"Tempest.Window";

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
  return reinterpret_cast<Window*>(window);
  }

void SystemApi::destroyWindow(SystemApi::Window *w) {
  DestroyWindow(HWND(w));
  }

void SystemApi::exec() {
  MSG  msg={};
  bool done=false;
  // main message loop
  while (!done) {
    PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE);
    if( msg.message==WM_QUIT ) { // check for a quit message
      done = true;  // if found, quit app
      } else {
      TranslateMessage(&msg);
      DispatchMessage(&msg);
      }
    //RedrawWindow(demo.window, nullptr, nullptr, RDW_INTERNALPAINT);
    }
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

    case WM_SIZE:
      if(wParam!=SIZE_MINIMIZED) {
        int width  = lParam & 0xffff;
        int height = (uint32_t(lParam) & 0xffff0000) >> 16;
        if(cb)
          cb->onResize(reinterpret_cast<SystemApi::Window*>(hWnd),uint32_t(width),uint32_t(height));
        }
      break;

    case WM_DESTROY: {
      PostQuitMessage(0);
      }
      break;
    default:
      if(cb)
        cb->onRender(reinterpret_cast<SystemApi::Window*>(hWnd));
    }
  return DefWindowProc( hWnd, msg, wParam, lParam );
  }
