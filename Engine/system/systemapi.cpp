#include "systemapi.h"

#include "api/windowsapi.h"
#include "api/x11api.h"
#include "eventdispatcher.h"

#include "exceptions/exception.h"
#include <Tempest/Event>
#include <Tempest/Window>

#include <thread>
#include <unordered_set>
#include <atomic>

using namespace Tempest;

static EventDispatcher dispatcher;

SystemApi::SystemApi() {
  }

void SystemApi::dispatchRender(Tempest::Window &w) {
  if(w.w()>0 && w.h()>0) {
    auto* wx = dynamic_cast<Tempest::Window*>(&w);
    wx->render();
    }
  }

SystemApi& SystemApi::inst() {
 #ifdef __WINDOWS__
  static WindowsApi api;
#elif defined(__LINUX__)
  static X11Api api;
#endif
  return api;
  }

void SystemApi::dispatchMouseDown(Tempest::Window &cb, MouseEvent &e) {
  dispatcher.dispatchMouseDown(cb,e);
  }

void SystemApi::dispatchMouseUp(Tempest::Window &cb, MouseEvent &e) {
  dispatcher.dispatchMouseUp(cb,e);
  }

void SystemApi::dispatchMouseMove(Tempest::Window &cb, MouseEvent &e) {
  dispatcher.dispatchMouseMove(cb,e);
  }

void SystemApi::dispatchMouseWheel(Tempest::Window &cb, MouseEvent &e) {
  dispatcher.dispatchMouseWheel(cb,e);
  }

void SystemApi::dispatchKeyDown(Tempest::Window &cb, KeyEvent &e, uint32_t scancode) {
  dispatcher.dispatchKeyDown(cb,e,scancode);
  }

void SystemApi::dispatchKeyUp(Tempest::Window &cb, KeyEvent &e, uint32_t scancode) {
  dispatcher.dispatchKeyUp(cb,e,scancode);
  }

SystemApi::Window *SystemApi::createWindow(Tempest::Window *owner, uint32_t width, uint32_t height) {
  return inst().implCreateWindow(owner,width,height);
  }

SystemApi::Window *SystemApi::createWindow(Tempest::Window *owner, ShowMode sm) {
  return inst().implCreateWindow(owner,sm);
  }

void SystemApi::destroyWindow(SystemApi::Window *w) {
  return inst().implDestroyWindow(w);
  }

void SystemApi::exit() {
  return inst().implExit();
  }

int SystemApi::exec(AppCallBack& cb) {
  return inst().implExec(cb);
  }

uint32_t SystemApi::width(SystemApi::Window *w) {
  return inst().implWidth(w);
  }

uint32_t SystemApi::height(SystemApi::Window *w) {
  return inst().implHeight(w);
  }

Rect SystemApi::windowClientRect(SystemApi::Window* w) {
  return inst().implWindowClientRect(w);
  }

bool SystemApi::setAsFullscreen(SystemApi::Window *wx,bool fullScreen) {
  return inst().implSetAsFullscreen(wx,fullScreen);
  }

bool SystemApi::isFullscreen(SystemApi::Window *w) {
  return inst().implIsFullscreen(w);
  }

void SystemApi::setCursorPosition(int x, int y) {
  return inst().implSetCursorPosition(x,y);
  }

void SystemApi::showCursor(bool show) {
  return inst().implShowCursor(show);
  }

