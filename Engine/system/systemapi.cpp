#include "systemapi.h"

#include "windowsapi.h"
#include "x11api.h"

#include "exceptions/exception.h"
#include <Tempest/Event>
#include <thread>
#include <unordered_set>
#include <atomic>

using namespace Tempest;

SystemApi::SystemApi() {
  }

SystemApi& SystemApi::inst() {
 #ifdef __WINDOWS__
  static WindowsApi api;
#elif defined(__LINUX__)
  static X11Api api;
#endif
  return api;
  }

SystemApi::Window *SystemApi::createWindow(SystemApi::WindowCallback *callback, uint32_t width, uint32_t height) {
  return inst().implCreateWindow(callback,width,height);
  }

SystemApi::Window *SystemApi::createWindow(SystemApi::WindowCallback *cb,ShowMode sm) {
  return inst().implCreateWindow(cb,sm);
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

