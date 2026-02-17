#include "systemapi.h"

#include "api/windowsapi.h"
#include "api/x11api.h"
#include "api/macosapi.h"
#include "api/iosapi.h"
#include "eventdispatcher.h"

#include <Tempest/Event>
#include <Tempest/Window>

using namespace Tempest;

static EventDispatcher dispatcher;

struct SystemApi::Data {
  std::vector<WindowsApi::TranslateKeyPair> keys;
  std::vector<WindowsApi::TranslateKeyPair> a, k0, f1;
  uint16_t                                  fkeysCount=1;
  };
SystemApi::Data SystemApi::m;

SystemApi::SystemApi() {
  }

float SystemApi::implUiScale(Window* w) {
  // default to 1.0, on platforms without hidpi implementation
  return 1;
  }

void SystemApi::implSetWindowTitle(Window *w, const char *utf8) {
  // TODO
  }

void SystemApi::setupKeyTranslate(const TranslateKeyPair k[], uint16_t funcCount ) {
  m.keys.clear();
  m.a. clear();
  m.k0.clear();
  m.f1.clear();
  m.fkeysCount = funcCount;

  for( size_t i=0; k[i].result!=Event::K_NoKey; ++i ){
    if( k[i].result==Event::K_A )
      m.a.push_back(k[i]); else
    if( k[i].result==Event::K_0 )
      m.k0.push_back(k[i]); else
    if( k[i].result==Event::K_F1 )
      m.f1.push_back(k[i]); else
      m.keys.push_back( k[i] );
    }

#ifndef __ANDROID__
  m.keys.shrink_to_fit();
  m.a. shrink_to_fit();
  m.k0.shrink_to_fit();
  m.f1.shrink_to_fit();
#endif
  }

void SystemApi::addOverlay(UiOverlay* ui) {
  dispatcher.addOverlay(ui);
  }

void SystemApi::takeOverlay(UiOverlay* ui) {
  dispatcher.takeOverlay(ui);
  }

uint16_t SystemApi::translateKey(uint64_t scancode) {
  for(size_t i=0; i<m.keys.size(); ++i)
    if( m.keys[i].src==scancode )
      return m.keys[i].result;

  for(size_t i=0; i<m.k0.size(); ++i)
    if( m.k0[i].src<=scancode &&
                     scancode<=m.k0[i].src+9 ){
      auto dx = (scancode-m.k0[i].src);
      return Event::KeyType(m.k0[i].result + dx);
      }

  uint16_t literalsCount = (Event::K_Z - Event::K_A);
  for(size_t i=0; i<m.a.size(); ++i)
    if(m.a[i].src<=scancode &&
                   scancode<=m.a[i].src+literalsCount ){
      auto dx = (scancode-m.a[i].src);
      return Event::KeyType(m.a[i].result + dx);
      }

  for(size_t i=0; i<m.f1.size(); ++i)
    if(m.f1[i].src<=scancode &&
                    scancode<=m.f1[i].src+m.fkeysCount ){
      auto dx = (scancode-m.f1[i].src);
      return Event::KeyType(m.f1[i].result+dx);
      }

  return Event::K_NoKey;
  }

SystemApi& SystemApi::inst() {
 #ifdef __WINDOWS__
  static WindowsApi api;
#elif defined(__UNIX__)
  static X11Api api;
#elif defined(__OSX__)
  static MacOSApi api;
#elif defined(__IOS__)
  static iOSApi api;
#endif
  return api;
  }

void SystemApi::dispatchOverlayRender(Tempest::Window &w, PaintEvent& e) {
  dispatcher.dispatchOverlayRender(w,e);
  }

void SystemApi::dispatchRender(Tempest::Window &w) {
  dispatcher.dispatchRender(w);
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

void SystemApi::dispatchResize(Tempest::Window& cb, SizeEvent& e) {
  dispatcher.dispatchResize(cb,e);
  }

void SystemApi::dispatchClose(Tempest::Window& cb, CloseEvent& e) {
  dispatcher.dispatchClose(cb,e);
  }

void SystemApi::dispatchFocus(Tempest::Window& cb, FocusEvent& e) {
  dispatcher.dispatchFocus(cb,e);
  }

bool SystemApi::isRunning() {
  return inst().implIsRunning();
  }

SystemApi::Window *SystemApi::createWindow(Tempest::Window *owner, uint32_t width, uint32_t height) {
  return inst().implCreateWindow(owner,width,height);
  }

SystemApi::Window *SystemApi::createWindow(Tempest::Window *owner, ShowMode sm) {
  return inst().implCreateWindow(owner,sm);
  }

void SystemApi::destroyWindow(SystemApi::Window *w) {
  dispatcher.dispatchDestroyWindow(w);
  return inst().implDestroyWindow(w);
  }

void SystemApi::exit() {
  return inst().implExit();
  }

int SystemApi::exec(AppCallBack& cb) {
  return inst().implExec(cb);
  }

void SystemApi::processEvent(AppCallBack& cb) {
  return inst().implProcessEvents(cb);
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

void SystemApi::setWindowTitle(Window *w, const char *utf8) {
  return inst().implSetWindowTitle(w, utf8);
  }

void SystemApi::setCursorPosition(SystemApi::Window *w, int x, int y) {
  return inst().implSetCursorPosition(w,x,y);
  }

void SystemApi::showCursor(SystemApi::Window *w, CursorShape show) {
  return inst().implShowCursor(w,show);
  }

float SystemApi::uiScale(Window* w) {
  return inst().implUiScale(w);
  }

SystemApi::ClipboardData SystemApi::clipboardData(SystemApi::Window *w) {
  return SystemApi::inst().implClipboardData(w);
}