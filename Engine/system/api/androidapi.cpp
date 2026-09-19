#include "androidapi.h"

#include <Tempest/Event>
#include <Tempest/Log>
#include <Tempest/Window>

#ifdef __ANDROID__

#include <android_native_app_glue.h>
#include <android/native_activity.h>
#include <android/native_window.h>

#include <exception>
#include <queue>
#include <thread>

using namespace Tempest;

namespace {

struct AndroidWindow {
  ANativeWindow*   nativeWindow = nullptr;
  Tempest::Window* owner        = nullptr;
  int32_t          width        = 0;
  int32_t          height       = 0;
  bool             fullscreen   = true;
  };

struct AppEvent {
  enum Type : uint8_t {
    Resize,
    Focus,
    Close,
    } type;

  int32_t width  = 0;
  int32_t height = 0;
  bool    focused = false;
  };

android_app*         app        = nullptr;
AndroidWindow*       mainWindow = nullptr;
std::queue<AppEvent> events;
bool                 running    = false;
bool                 resumed    = false;
bool                 focused    = false;
bool                 active     = false;
bool                 hasWindow  = false;

void pushFocus() {
  const bool next = resumed && focused;
  if(active==next)
    return;
  active = next;
  events.push({AppEvent::Focus,0,0,active});
  }

void updateWindow(bool force) {
  if(app==nullptr || app->window==nullptr)
    return;

  hasWindow = true;
  if(mainWindow==nullptr)
    return;

  const int32_t width  = ANativeWindow_getWidth(app->window);
  const int32_t height = ANativeWindow_getHeight(app->window);
  mainWindow->nativeWindow = app->window;
  if(!force && mainWindow->width==width && mainWindow->height==height)
    return;

  mainWindow->width  = width;
  mainWindow->height = height;
  events.push({AppEvent::Resize,width,height,false});
  }

void onAppCmd(android_app*, int32_t cmd) {
  switch(cmd) {
    case APP_CMD_INIT_WINDOW:
      updateWindow(true);
      break;
    case APP_CMD_TERM_WINDOW:
      hasWindow = false;
      if(mainWindow!=nullptr)
        mainWindow->nativeWindow = nullptr;
      break;
    case APP_CMD_WINDOW_RESIZED:
    case APP_CMD_CONFIG_CHANGED:
      updateWindow(false);
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
      events.push({AppEvent::Close});
      running = false;
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
  if(mainWindow==nullptr)
    mainWindow = new AndroidWindow();
  mainWindow->owner = owner;
  updateWindow(true);
  return reinterpret_cast<SystemApi::Window*>(mainWindow);
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

void AndroidApi::implDestroyWindow(SystemApi::Window* w) {
  if(mainWindow!=nullptr && reinterpret_cast<SystemApi::Window*>(mainWindow)==w)
    mainWindow->owner = nullptr;
  }

void AndroidApi::implExit() {
  running = false;
  }

Rect AndroidApi::implWindowClientRect(SystemApi::Window* w) {
  const auto window = reinterpret_cast<AndroidWindow*>(w);
  if(window==nullptr)
    return {};
  return Rect(0,0,window->width,window->height);
  }

bool AndroidApi::implSetAsFullscreen(SystemApi::Window* w, bool fullscreen) {
  const auto window = reinterpret_cast<AndroidWindow*>(w);
  if(window!=nullptr)
    window->fullscreen = fullscreen;
  return true;
  }

bool AndroidApi::implIsFullscreen(SystemApi::Window* w) {
  const auto window = reinterpret_cast<AndroidWindow*>(w);
  return window==nullptr || window->fullscreen;
  }

void AndroidApi::implSetCursorPosition(SystemApi::Window*, int, int) {
  }

void AndroidApi::implShowCursor(SystemApi::Window*, CursorShape) {
  }

bool AndroidApi::implIsRunning() {
  return running;
  }

int AndroidApi::implExec(AppCallBack& cb) {
  running = true;
  while(running) {
    pollAndroid(active && hasWindow ? 0 : -1);
    implProcessEvents(cb);
    if(active && hasWindow) {
      if(cb.onTimer()==0)
        std::this_thread::yield();
      }
    }
  return 0;
  }

void AndroidApi::implProcessEvents(AppCallBack&) {
  pollAndroid(0);
  if(mainWindow==nullptr || mainWindow->owner==nullptr)
    return;

  auto& window = *mainWindow->owner;
  while(!events.empty()) {
    const AppEvent event = events.front();
    events.pop();
    switch(event.type) {
      case AppEvent::Resize: {
        SizeEvent e(event.width,event.height);
        dispatchResize(window,e,true);
        break;
        }
      case AppEvent::Focus: {
        FocusEvent e(event.focused,Event::FocusReason::UnknownReason);
        dispatchFocus(window,e);
        break;
        }
      case AppEvent::Close: {
        CloseEvent e;
        dispatchClose(window,e);
        break;
        }
      }
    }

  if(active && hasWindow)
    dispatchRender(window);
  }

void AndroidApi::implSetWindowTitle(SystemApi::Window*, const char*) {
  }

int main(int argc, const char** argv);

extern "C" void android_main(android_app* state) {
  app = state;
  app->onAppCmd = onAppCmd;

  while(!hasWindow && app->destroyRequested==0)
    pollAndroid(-1);

  const char* argv[] = {"app",nullptr};
  try {
    if(app->destroyRequested==0)
      main(1,argv);
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
    pollAndroid(-1);

  delete mainWindow;
  mainWindow = nullptr;
  events = {};
  running = false;
  resumed = false;
  focused = false;
  active = false;
  hasWindow = false;
  app = nullptr;
  }

#endif
