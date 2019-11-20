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

struct SystemApi::KeyInf {
  std::vector<WindowsApi::TranslateKeyPair> keys;
  std::vector<WindowsApi::TranslateKeyPair> a, k0, f1;
  uint16_t                                  fkeysCount=1;
  };
SystemApi::KeyInf SystemApi::ki;

SystemApi::SystemApi() {
  }

void SystemApi::setupKeyTranslate(const TranslateKeyPair k[], uint16_t funcCount ) {
  ki.keys.clear();
  ki.a. clear();
  ki.k0.clear();
  ki.f1.clear();
  ki.fkeysCount = funcCount;

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

