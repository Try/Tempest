#include "androidapi.h"

#ifdef __ANDROID__

#include <android_native_app_glue.h>

#include <Tempest/Event>
#include <Tempest/Log>
#include <Tempest/Window>

#include <android/native_activity.h>
#include <android/native_window.h>
#include <android/input.h>

#include <atomic>
#include <cassert>
#include <cstdlib>
#include <dlfcn.h>
#include <exception>
#include <thread>
#include <vector>

using namespace Tempest;

extern "C" void android_main(android_app* state);

static android_app*     app        = nullptr;
static Tempest::Window* mainWindow = nullptr;
static std::atomic_bool isExit     = false;
static bool            resumed    = false;
static bool            focused    = false;
static bool            active     = false;
static bool            hasWindow  = false;

struct TouchPoint {
  int32_t id = -1;
  Point   pos;
  };
static std::vector<TouchPoint> touches;

std::filesystem::path AndroidApi::internalDataPath() {
  assert(app!=nullptr && app->activity!=nullptr);
  return app->activity->internalDataPath;
  }

std::filesystem::path AndroidApi::externalDataPath() {
  assert(app!=nullptr && app->activity!=nullptr);
  const char* path = app->activity->externalDataPath;
  return path!=nullptr ? path : "";
  }

void AndroidApi::updateFocus() {
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

  SizeEvent event(ANativeWindow_getWidth(app->window),ANativeWindow_getHeight(app->window));
  AndroidApi::dispatchResize(*mainWindow,event);
  }

void AndroidApi::onAppCmd(void*, int32_t cmd) {
  switch(cmd) {
    case APP_CMD_INIT_WINDOW:
      updateWindow();
      break;
    case APP_CMD_WINDOW_RESIZED:
    case APP_CMD_CONFIG_CHANGED:
      updateWindow();
      break;
    case APP_CMD_GAINED_FOCUS:
      focused = true;
      updateFocus();
      break;
    case APP_CMD_LOST_FOCUS:
      focused = false;
      updateFocus();
      break;
    case APP_CMD_RESUME:
      resumed = true;
      updateFocus();
      break;
    case APP_CMD_PAUSE:
      resumed = false;
      updateFocus();
      break;
    case APP_CMD_TERM_WINDOW:
    case APP_CMD_DESTROY:
      hasWindow = false;
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

int32_t AndroidApi::onInputEvent(const void* input) {
  const auto event = static_cast<const AInputEvent*>(input);
  if(AInputEvent_getType(event)!=AINPUT_EVENT_TYPE_MOTION ||
     (AInputEvent_getSource(event) & AINPUT_SOURCE_TOUCHSCREEN)!=AINPUT_SOURCE_TOUCHSCREEN)
    return 0;

  const int32_t action = AMotionEvent_getAction(event);
  const int32_t kind = action & AMOTION_EVENT_ACTION_MASK;
  Event::Type type;
  switch(kind) {
    case AMOTION_EVENT_ACTION_DOWN:
    case AMOTION_EVENT_ACTION_POINTER_DOWN:
      type = Event::MouseDown;
      break;
    case AMOTION_EVENT_ACTION_UP:
    case AMOTION_EVENT_ACTION_POINTER_UP:
    case AMOTION_EVENT_ACTION_CANCEL:
      type = Event::MouseUp;
      break;
    case AMOTION_EVENT_ACTION_MOVE:
      type = Event::MouseMove;
      break;
    default:
      return 0;
    }

  const bool all = kind==AMOTION_EVENT_ACTION_MOVE || kind==AMOTION_EVENT_ACTION_CANCEL;
  const size_t first = all ? 0 : size_t((action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT);
  const size_t end = all ? AMotionEvent_getPointerCount(event) : first+1;
  if(kind==AMOTION_EVENT_ACTION_DOWN)
    touches.clear();
  for(size_t i=first; i<end && mainWindow!=nullptr && !isExit.load(); ++i) {
    const int32_t id = AMotionEvent_getPointerId(event,i);
    const Point pos(int(AMotionEvent_getX(event,i)), int(AMotionEvent_getY(event,i)));
    size_t slot = 0;
    while(slot<touches.size() && touches[slot].id!=id)
      ++slot;
    if(type==Event::MouseDown && slot==touches.size()) {
      slot = 0;
      while(slot<touches.size() && touches[slot].id!=-1)
        ++slot;
      if(slot==touches.size())
        touches.emplace_back();
      touches[slot].id = id;
      }
    if(slot==touches.size())
      continue;
    if(type==Event::MouseMove && touches[slot].pos==pos)
      continue;
    touches[slot].pos = pos;
    if(type==Event::MouseUp)
      touches[slot].id = -1;
    MouseEvent mouse(pos.x, pos.y, Event::ButtonLeft, Event::M_NoModifier, 0, uint32_t(slot), type);
    if(type==Event::MouseDown)
      SystemApi::dispatchMouseDown(*mainWindow,mouse);
    else if(type==Event::MouseUp)
      SystemApi::dispatchMouseUp(*mainWindow,mouse);
    else
      SystemApi::dispatchMouseMove(*mainWindow,mouse);
    }
  return 1;
  }

static void pollAndroid(android_app* state, int timeout) {
  int                  pending = 0;
  android_poll_source* source  = nullptr;
  while(ALooper_pollOnce(timeout,nullptr,&pending,reinterpret_cast<void**>(&source))>=0) {
    if(source!=nullptr)
      source->process(state,source);
    if(isExit.load())
      break;
    timeout = 0;
    }
  }

SystemApi::Window* AndroidApi::createAndroidWindow(Tempest::Window* owner) {
  if(mainWindow!=nullptr)
    return nullptr;
  app->onAppCmd = [](android_app* state, int32_t cmd) { onAppCmd(state,cmd); };
  app->onInputEvent = [](android_app*, AInputEvent* event) { return onInputEvent(event); };
  while(!hasWindow && !isExit.load() && app->destroyRequested==0)
    pollAndroid(app,-1);
  if(isExit.load() || app->destroyRequested!=0)
    return nullptr;
  mainWindow = owner;
  ANativeWindow_acquire(app->window);
  return reinterpret_cast<SystemApi::Window*>(app->window);
  }

SystemApi::Window* AndroidApi::implCreateWindow(Tempest::Window* owner, uint32_t, uint32_t) {
  return createAndroidWindow(owner);
  }

SystemApi::Window* AndroidApi::implCreateWindow(Tempest::Window* owner, ShowMode) {
  return createAndroidWindow(owner);
  }

void AndroidApi::implDestroyWindow(SystemApi::Window* w) {
  ANativeWindow_release(reinterpret_cast<ANativeWindow*>(w));
  mainWindow = nullptr;
  touches.clear();
  }

void AndroidApi::implExit() {
  isExit.store(true);
  ALooper_wake(app->looper);
  }

Rect AndroidApi::implWindowClientRect(SystemApi::Window* w) {
  const auto window = reinterpret_cast<ANativeWindow*>(w);
  return Rect(0,0,ANativeWindow_getWidth(window),ANativeWindow_getHeight(window));
  }

bool AndroidApi::implSetAsFullscreen(SystemApi::Window*, bool) {
  // TODO: toggle Android immersive mode.
  return false;
  }

bool AndroidApi::implIsFullscreen(SystemApi::Window*) {
  // TODO: query Android immersive mode.
  return true;
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

static int runMain() {
  Dl_info module = {};
  if(dladdr(reinterpret_cast<void*>(&android_main),&module)==0) {
    Log::e("Unable to locate the application library");
    return EXIT_FAILURE;
    }
  void* self = dlopen(module.dli_fname,RTLD_NOW);
  if(self==nullptr) {
    const char* error = dlerror();
    Log::e("Unable to open the application library: ",error==nullptr ? "unknown error" : error);
    return EXIT_FAILURE;
    }
  using Main = int(*)(int,char**);
  auto entry = reinterpret_cast<Main>(dlsym(self,"main"));
  if(entry==nullptr) {
    const char* error = dlerror();
    Log::e("Unable to find the application entry point: ",error==nullptr ? "unknown error" : error);
    dlclose(self);
    return EXIT_FAILURE;
    }
  // NativeActivity owns the library for the duration of android_main.
  dlclose(self);
  char  arg0[] = "app";
  char* argv[] = {arg0,nullptr};
  return entry(1,argv);
  }

extern "C" void android_main(android_app* state) {
  static std::atomic_flag started = ATOMIC_FLAG_INIT;
  if(started.test_and_set()) {
    ANativeActivity_finish(state->activity);
    while(state->destroyRequested==0)
      pollAndroid(state,-1);
    return;
    }
  app = state;
  int result = EXIT_FAILURE;
  try {
    result = runMain();
    }
  catch(const std::exception& e) {
    Log::e("Unhandled native exception: ",e.what());
    }
  catch(...) {
    Log::e("Unhandled native exception");
    }

  if(app->destroyRequested==0)
    ANativeActivity_finish(app->activity);
  // Re-entering main would reuse application statics from the previous run.
  // The application has unwound; process-wide destructors can race Android runtime threads.
  std::_Exit(result);
  }

#endif
