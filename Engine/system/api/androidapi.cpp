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
#include <mutex>
#include <thread>

using namespace Tempest;

extern "C" void android_main(android_app* state);

static android_app*     app        = nullptr;
static Tempest::Window* mainWindow = nullptr;
static std::atomic_bool isExit     = false;
static bool            resumed    = false;
static bool            focused    = false;
static bool            active     = false;
static bool            hasWindow  = false;
static bool            fullscreen = true;

void AndroidApi::pushFocus() {
  const bool next = resumed && focused;
  if(active==next)
    return;
  active = next;
  if(mainWindow!=nullptr) {
    FocusEvent event(active,Event::FocusReason::UnknownReason);
    AndroidApi::dispatchFocus(*mainWindow,event);
    }
  }

void AndroidApi::updateWindow() {
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

void AndroidApi::onAppCmd(void*, int32_t cmd) {
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
      isExit.store(true);
      break;
    default:
      break;
    }
  }

static void pollAndroid(android_app* state, int timeout) {
  int                  pending = 0;
  android_poll_source* source  = nullptr;
  while(ALooper_pollOnce(timeout,nullptr,&pending,reinterpret_cast<void**>(&source))>=0) {
    if(source!=nullptr)
      source->process(state,source);
    timeout = 0;
    }
  }

SystemApi::Window* AndroidApi::createAndroidWindow(Tempest::Window* owner) {
  if(mainWindow!=nullptr)
    return nullptr;
  app->onAppCmd = [](android_app* state, int32_t cmd) { onAppCmd(state,cmd); };
  while(!hasWindow && !isExit.load() && app->destroyRequested==0)
    pollAndroid(app,-1);
  if(isExit.load() || app->destroyRequested!=0)
    return nullptr;
  mainWindow = owner;
  return reinterpret_cast<SystemApi::Window*>(app->window);
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
  isExit.store(true);
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
  return !isExit.load();
  }

int AndroidApi::implExec(AppCallBack& cb) {
  while(!isExit.load()) {
    implProcessEvents(cb);
    }
  return 0;
  }

void AndroidApi::implProcessEvents(AppCallBack& cb) {
  if(isExit.load())
    return;
  pollAndroid(app,active && hasWindow ? 0 : -1);
  if(isExit.load())
    return;
  if(mainWindow!=nullptr && active && hasWindow)
    dispatchRender(*mainWindow);
  if(active && hasWindow && !isExit.load() && cb.onTimer()==0)
    std::this_thread::yield();
  }

void AndroidApi::implSetWindowTitle(SystemApi::Window*, const char*) {
  // TODO: update the activity title through JNI.
  }

static void runMain() {
  Dl_info module = {};
  if(dladdr(reinterpret_cast<void*>(&android_main),&module)==0) {
    Log::e("Unable to locate the application library");
    return;
    }
  void* self = dlopen(module.dli_fname,RTLD_NOW);
  if(self==nullptr) {
    const char* error = dlerror();
    Log::e("Unable to open the application library: ",error==nullptr ? "unknown error" : error);
    return;
    }
  using Main = int(*)(int,char**);
  auto entry = reinterpret_cast<Main>(dlsym(self,"main"));
  if(entry==nullptr) {
    const char* error = dlerror();
    Log::e("Unable to find the application entry point: ",error==nullptr ? "unknown error" : error);
    dlclose(self);
    return;
    }
  // NativeActivity owns the library for the duration of android_main.
  dlclose(self);
  char  arg0[] = "app";
  char* argv[] = {arg0,nullptr};
  entry(1,argv);
  }

extern "C" void android_main(android_app* state) {
  static std::mutex sync;
  std::unique_lock<std::mutex> guard(sync,std::defer_lock);
  bool initialFocus = false;
  state->userData = &initialFocus;
  state->onAppCmd = [](android_app* state, int32_t cmd) {
    if(cmd==APP_CMD_GAINED_FOCUS || cmd==APP_CMD_LOST_FOCUS)
      *static_cast<bool*>(state->userData) = (cmd==APP_CMD_GAINED_FOCUS);
    };
  // Pump the replacement activity while waiting so the UI thread can destroy the previous one.
  while(!guard.try_lock()) {
    pollAndroid(state,10);
    if(state->destroyRequested!=0)
      return;
    }
  state->onAppCmd = nullptr;
  state->userData = nullptr;
  app = state;
  // NativeActivity can restart without restarting the process.
  isExit.store(false);
  resumed    = (state->activityState==APP_CMD_RESUME);
  focused    = initialFocus;
  active     = resumed && focused;
  hasWindow  = (state->window!=nullptr);
  fullscreen = true;
  try {
    runMain();
    }
  catch(const std::exception& e) {
    Log::e("Unhandled native exception: ",e.what());
    }
  catch(...) {
    Log::e("Unhandled native exception");
    }

  if(app->destroyRequested==0)
    ANativeActivity_finish(app->activity);
  while(app->destroyRequested==0)
    pollAndroid(app,-1);
  }

#endif
