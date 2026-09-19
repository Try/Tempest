#include "androidapi.h"

#ifdef __ANDROID__

#include <android_native_app_glue.h>

#include <Tempest/Event>
#include <Tempest/Log>
#include <Tempest/Window>

#include <android/native_activity.h>
#include <android/native_window.h>

#include <atomic>
#include <dlfcn.h>
#include <exception>
#include <thread>

using namespace Tempest;

extern "C" void android_main(android_app* state);

namespace {

android_app*         app        = nullptr;
Tempest::Window*     mainWindow = nullptr;
std::atomic_bool     running    = false;
bool                 resumed    = false;
bool                 focused    = false;
bool                 active     = false;
bool                 hasWindow  = false;
bool                 fullscreen = true;

void pushFocus() {
  const bool next = resumed && focused;
  if(active==next)
    return;
  active = next;
  if(mainWindow!=nullptr) {
    FocusEvent event(active,Event::FocusReason::UnknownReason);
    AndroidApi::dispatchFocus(*mainWindow,event);
    }
  }

void updateWindow() {
  if(app==nullptr || app->window==nullptr)
    return;

  hasWindow = true;
  if(mainWindow==nullptr)
    return;

  auto window = reinterpret_cast<SystemApi::Window*>(app->window);
  AndroidApi::setWindowHandle(*mainWindow,window);
  SizeEvent event(ANativeWindow_getWidth(app->window),ANativeWindow_getHeight(app->window));
  AndroidApi::dispatchResize(*mainWindow,event);
  }

void onAppCmd(android_app*, int32_t cmd) {
  switch(cmd) {
    case APP_CMD_INIT_WINDOW:
      updateWindow();
      break;
    case APP_CMD_TERM_WINDOW:
      hasWindow = false;
      break;
    case APP_CMD_WINDOW_RESIZED:
    case APP_CMD_CONFIG_CHANGED:
      updateWindow();
      break;
    case APP_CMD_GAINED_FOCUS:
      focused = true;
      pushFocus();
      break;
    case APP_CMD_LOST_FOCUS:
      focused = false;
      pushFocus();
      break;
    case APP_CMD_RESUME:
      resumed = true;
      pushFocus();
      break;
    case APP_CMD_PAUSE:
      resumed = false;
      pushFocus();
      break;
    case APP_CMD_DESTROY:
      if(mainWindow!=nullptr) {
        CloseEvent event;
        AndroidApi::dispatchClose(*mainWindow,event);
        }
      running.store(false);
      break;
    default:
      break;
    }
  }

void pollAndroid(int timeout) {
  int                  pending = 0;
  android_poll_source* source  = nullptr;
  while(ALooper_pollOnce(timeout,nullptr,&pending,reinterpret_cast<void**>(&source))>=0) {
    if(source!=nullptr)
      source->process(app,source);
    timeout = 0;
    }
  }

SystemApi::Window* createAndroidWindow(Tempest::Window* owner) {
  mainWindow = owner;
  return reinterpret_cast<SystemApi::Window*>(app->window);
  }

}

AndroidApi::AndroidApi() {
  }

SystemApi::Window* AndroidApi::implCreateWindow(Tempest::Window* owner, uint32_t, uint32_t) {
  return createAndroidWindow(owner);
  }

SystemApi::Window* AndroidApi::implCreateWindow(Tempest::Window* owner, ShowMode) {
  return createAndroidWindow(owner);
  }

void AndroidApi::implDestroyWindow(SystemApi::Window*) {
  mainWindow = nullptr;
  }

void AndroidApi::implExit() {
  running.store(false);
  ALooper_wake(app->looper);
  }

Rect AndroidApi::implWindowClientRect(SystemApi::Window* w) {
  const auto window = reinterpret_cast<ANativeWindow*>(w);
  return Rect(0,0,ANativeWindow_getWidth(window),ANativeWindow_getHeight(window));
  }

bool AndroidApi::implSetAsFullscreen(SystemApi::Window*, bool value) {
  fullscreen = value;
  return true;
  }

bool AndroidApi::implIsFullscreen(SystemApi::Window*) {
  return fullscreen;
  }

void AndroidApi::implSetCursorPosition(SystemApi::Window*, int, int) {
  }

void AndroidApi::implShowCursor(SystemApi::Window*, CursorShape) {
  }

bool AndroidApi::implIsRunning() {
  return running.load();
  }

int AndroidApi::implExec(AppCallBack& cb) {
  running.store(true);
  while(running.load()) {
    implProcessEvents(cb);
    if(active && hasWindow) {
      if(cb.onTimer()==0)
        std::this_thread::yield();
      }
    }
  return 0;
  }

void AndroidApi::implProcessEvents(AppCallBack&) {
  pollAndroid(active && hasWindow ? 0 : -1);
  if(mainWindow!=nullptr && active && hasWindow)
    dispatchRender(*mainWindow);
  }

void AndroidApi::implSetWindowTitle(SystemApi::Window*, const char*) {
  // TODO: update the activity title through JNI.
  }

extern "C" void android_main(android_app* state) {
  app = state;
  app->onAppCmd = onAppCmd;

  while(!hasWindow && app->destroyRequested==0)
    pollAndroid(-1);

  Dl_info module = {};
  void* self = nullptr;
  if(dladdr(reinterpret_cast<void*>(&android_main),&module)!=0)
    self = dlopen(module.dli_fname,RTLD_NOW);
  if(self==nullptr) {
    const char* error = dlerror();
    Log::e("Unable to open the application library: ",error==nullptr ? "unknown error" : error);
    }
  else {
    using Main = int(*)(int,char**);
    auto entry = reinterpret_cast<Main>(dlsym(self,"main"));
    if(entry==nullptr) {
      const char* error = dlerror();
      Log::e("Unable to find the application entry point: ",error==nullptr ? "unknown error" : error);
      }
    else {
      char  arg0[] = "app";
      char* argv[] = {arg0,nullptr};
      try {
        if(app->destroyRequested==0)
          entry(1,argv);
        }
      catch(const std::exception& e) {
        Log::e("Unhandled native exception: ",e.what());
        }
      catch(...) {
        Log::e("Unhandled native exception");
        }
      }
    dlclose(self);
    }

  if(app->destroyRequested==0)
    ANativeActivity_finish(app->activity);
  while(app->destroyRequested==0)
    pollAndroid(-1);

  }

#endif
