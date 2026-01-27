#include "androidapi.h"

#include <Tempest/Platform>
#include <Tempest/Log>

#ifdef __ANDROID__

#include <android_native_app_glue.h>
#include <android/native_activity.h>
#include <android/native_window.h>
#include <android/input.h>
#include <android/looper.h>
#include <android/log.h>

#include <Tempest/Window>
#include <Tempest/Event>

#include <string>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <queue>

#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO, "Tempest", __VA_ARGS__))
#define LOGW(...) ((void)__android_log_print(ANDROID_LOG_WARN, "Tempest", __VA_ARGS__))
#define LOGE(...) ((void)__android_log_print(ANDROID_LOG_ERROR, "Tempest", __VA_ARGS__))

using namespace Tempest;

// Forward declarations
static void onAppCmd(struct android_app* app, int32_t cmd);
static int32_t onInputEvent(struct android_app* app, AInputEvent* event);

struct AndroidWindow {
  ANativeWindow*       nativeWindow = nullptr;
  Tempest::Window*     owner        = nullptr;
  int32_t              width        = 0;
  int32_t              height       = 0;
  float                density      = 1.0f;
  std::atomic_bool     hasPendingFrame{false};
  bool                 isFullscreen = true;

  struct TouchState {
    struct Touch {
      int32_t id;
      float   x;
      float   y;
      bool    active;
    };
    std::vector<Touch> touches;

    TouchState() {
      touches.reserve(10);
    }

    int add(int32_t id, float x, float y) {
      for (auto& t : touches) {
        if (!t.active) {
          t.id = id;
          t.x = x;
          t.y = y;
          t.active = true;
          return static_cast<int>(&t - touches.data());
        }
      }
      touches.push_back({id, x, y, true});
      return static_cast<int>(touches.size() - 1);
    }

    int find(int32_t id) const {
      for (size_t i = 0; i < touches.size(); ++i) {
        if (touches[i].active && touches[i].id == id)
          return static_cast<int>(i);
      }
      return -1;
    }

    int update(int32_t id, float x, float y) {
      int idx = find(id);
      if (idx >= 0) {
        touches[idx].x = x;
        touches[idx].y = y;
      }
      return idx;
    }

    int remove(int32_t id) {
      int idx = find(id);
      if (idx >= 0) {
        touches[idx].active = false;
      }
      return idx;
    }
  };
  TouchState touch;
};

// Global state
static struct android_app*  g_app          = nullptr;
static AndroidWindow*       g_mainWindow   = nullptr;
static std::atomic_bool     g_isRunning{false};
static std::atomic_bool     g_isActive{false};
static std::atomic_bool     g_hasWindow{false};

// Event queue for cross-thread communication
struct AppEvent {
  enum Type {
    None,
    Resize,
    TouchDown,
    TouchMove,
    TouchUp,
    KeyDown,
    KeyUp,
    Focus,
    Close
  };
  Type type = None;

  union {
    struct { int32_t w, h; } resize;
    struct { int x, y, pointerId; } touch;
    struct { uint32_t keyCode; } key;
    struct { bool gained; } focus;
  } data;
};

static std::mutex              g_eventMutex;
static std::queue<AppEvent>    g_eventQueue;

static void pushEvent(const AppEvent& evt) {
  std::lock_guard<std::mutex> lock(g_eventMutex);
  g_eventQueue.push(evt);
}

static bool popEvent(AppEvent& evt) {
  std::lock_guard<std::mutex> lock(g_eventMutex);
  if (g_eventQueue.empty())
    return false;
  evt = g_eventQueue.front();
  g_eventQueue.pop();
  return true;
}

// Handle application lifecycle commands
static void onAppCmd(struct android_app* app, int32_t cmd) {
  switch (cmd) {
    case APP_CMD_INIT_WINDOW:
      if (app->window != nullptr) {
        g_hasWindow.store(true);
        if (g_mainWindow != nullptr) {
          g_mainWindow->nativeWindow = app->window;
          g_mainWindow->width  = ANativeWindow_getWidth(app->window);
          g_mainWindow->height = ANativeWindow_getHeight(app->window);

          AppEvent evt;
          evt.type = AppEvent::Resize;
          evt.data.resize.w = g_mainWindow->width;
          evt.data.resize.h = g_mainWindow->height;
          pushEvent(evt);
        }
        LOGI("Window initialized: %dx%d", ANativeWindow_getWidth(app->window), ANativeWindow_getHeight(app->window));
      }
      break;

    case APP_CMD_TERM_WINDOW:
      g_hasWindow.store(false);
      if (g_mainWindow != nullptr) {
        g_mainWindow->nativeWindow = nullptr;
      }
      LOGI("Window terminated");
      break;

    case APP_CMD_GAINED_FOCUS:
      g_isActive.store(true);
      {
        AppEvent evt;
        evt.type = AppEvent::Focus;
        evt.data.focus.gained = true;
        pushEvent(evt);
      }
      LOGI("Focus gained");
      break;

    case APP_CMD_LOST_FOCUS:
      g_isActive.store(false);
      {
        AppEvent evt;
        evt.type = AppEvent::Focus;
        evt.data.focus.gained = false;
        pushEvent(evt);
      }
      LOGI("Focus lost");
      break;

    case APP_CMD_START:
      LOGI("App started");
      break;

    case APP_CMD_RESUME:
      g_isActive.store(true);
      LOGI("App resumed");
      break;

    case APP_CMD_PAUSE:
      g_isActive.store(false);
      LOGI("App paused");
      break;

    case APP_CMD_STOP:
      LOGI("App stopped");
      break;

    case APP_CMD_DESTROY:
      g_isRunning.store(false);
      {
        AppEvent evt;
        evt.type = AppEvent::Close;
        pushEvent(evt);
      }
      LOGI("App destroyed");
      break;

    case APP_CMD_CONFIG_CHANGED:
      LOGI("Config changed");
      // Window resize is handled in APP_CMD_WINDOW_RESIZED or when we get a new window
      break;

    case APP_CMD_WINDOW_RESIZED:
      if (app->window != nullptr && g_mainWindow != nullptr) {
        int32_t newW = ANativeWindow_getWidth(app->window);
        int32_t newH = ANativeWindow_getHeight(app->window);
        LOGI("Window resized: %dx%d -> %dx%d", g_mainWindow->width, g_mainWindow->height, newW, newH);
        if (newW != g_mainWindow->width || newH != g_mainWindow->height) {
          g_mainWindow->width  = newW;
          g_mainWindow->height = newH;

          AppEvent evt;
          evt.type = AppEvent::Resize;
          evt.data.resize.w = newW;
          evt.data.resize.h = newH;
          pushEvent(evt);
        }
      }
      break;

    default:
      break;
  }
}

// Handle input events
static int32_t onInputEvent(struct android_app* app, AInputEvent* event) {
  if (g_mainWindow == nullptr || g_mainWindow->owner == nullptr)
    return 0;

  int32_t eventType = AInputEvent_getType(event);

  if (eventType == AINPUT_EVENT_TYPE_MOTION) {
    int32_t action = AMotionEvent_getAction(event);
    int32_t actionMasked = action & AMOTION_EVENT_ACTION_MASK;
    int32_t pointerIndex = (action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;

    float x = AMotionEvent_getX(event, pointerIndex);
    float y = AMotionEvent_getY(event, pointerIndex);
    int32_t pointerId = AMotionEvent_getPointerId(event, pointerIndex);

    AppEvent evt;
    evt.data.touch.x = static_cast<int>(x);
    evt.data.touch.y = static_cast<int>(y);

    switch (actionMasked) {
      case AMOTION_EVENT_ACTION_DOWN:
      case AMOTION_EVENT_ACTION_POINTER_DOWN:
        evt.type = AppEvent::TouchDown;
        evt.data.touch.pointerId = g_mainWindow->touch.add(pointerId, x, y);
        pushEvent(evt);
        return 1;

      case AMOTION_EVENT_ACTION_UP:
      case AMOTION_EVENT_ACTION_POINTER_UP:
        evt.type = AppEvent::TouchUp;
        evt.data.touch.pointerId = g_mainWindow->touch.remove(pointerId);
        pushEvent(evt);
        return 1;

      case AMOTION_EVENT_ACTION_MOVE:
        // Handle all pointers that moved
        for (size_t i = 0; i < AMotionEvent_getPointerCount(event); ++i) {
          int32_t pId = AMotionEvent_getPointerId(event, i);
          float px = AMotionEvent_getX(event, i);
          float py = AMotionEvent_getY(event, i);
          int idx = g_mainWindow->touch.update(pId, px, py);
          if (idx >= 0) {
            evt.type = AppEvent::TouchMove;
            evt.data.touch.x = static_cast<int>(px);
            evt.data.touch.y = static_cast<int>(py);
            evt.data.touch.pointerId = idx;
            pushEvent(evt);
          }
        }
        return 1;

      case AMOTION_EVENT_ACTION_CANCEL:
        // Cancel all touches
        for (auto& t : g_mainWindow->touch.touches) {
          if (t.active) {
            evt.type = AppEvent::TouchUp;
            evt.data.touch.x = static_cast<int>(t.x);
            evt.data.touch.y = static_cast<int>(t.y);
            evt.data.touch.pointerId = g_mainWindow->touch.find(t.id);
            t.active = false;
            pushEvent(evt);
          }
        }
        return 1;

      default:
        break;
    }
  }
  else if (eventType == AINPUT_EVENT_TYPE_KEY) {
    int32_t action  = AKeyEvent_getAction(event);
    int32_t keyCode = AKeyEvent_getKeyCode(event);

    // Handle back button
    if (keyCode == AKEYCODE_BACK) {
      if (action == AKEY_EVENT_ACTION_UP) {
        // Could dispatch as Escape key or handle app exit
        AppEvent evt;
        evt.type = AppEvent::KeyUp;
        evt.data.key.keyCode = keyCode;
        pushEvent(evt);
      }
      return 1;
    }

    AppEvent evt;
    evt.data.key.keyCode = keyCode;
    if (action == AKEY_EVENT_ACTION_DOWN) {
      evt.type = AppEvent::KeyDown;
      pushEvent(evt);
      return 1;
    }
    else if (action == AKEY_EVENT_ACTION_UP) {
      evt.type = AppEvent::KeyUp;
      pushEvent(evt);
      return 1;
    }
  }

  return 0;
}

static SystemApi::Window* createWindow(Tempest::Window* owner, uint32_t w, uint32_t h, SystemApi::ShowMode mode) {
  if (g_mainWindow == nullptr) {
    g_mainWindow = new AndroidWindow();
  }

  g_mainWindow->owner = owner;

  if (g_app != nullptr && g_app->window != nullptr) {
    g_mainWindow->nativeWindow = g_app->window;
    g_mainWindow->width  = ANativeWindow_getWidth(g_app->window);
    g_mainWindow->height = ANativeWindow_getHeight(g_app->window);
  }

  g_mainWindow->hasPendingFrame.store(true);
  return reinterpret_cast<SystemApi::Window*>(g_mainWindow);
}

AndroidApi::AndroidApi() {
  // Initialization is done via android_main
}

SystemApi::Window* AndroidApi::implCreateWindow(Tempest::Window* owner, uint32_t width, uint32_t height) {
  return ::createWindow(owner, width, height, ShowMode::Maximized);
}

SystemApi::Window* AndroidApi::implCreateWindow(Tempest::Window* owner, SystemApi::ShowMode sm) {
  return ::createWindow(owner, 800, 600, sm);
}

void AndroidApi::implDestroyWindow(SystemApi::Window* w) {
  if (g_mainWindow != nullptr && reinterpret_cast<SystemApi::Window*>(g_mainWindow) == w) {
    g_mainWindow->owner = nullptr;
  }
}

void AndroidApi::implExit() {
  g_isRunning.store(false);
  if (g_app != nullptr) {
    ANativeActivity_finish(g_app->activity);
  }
}

Tempest::Rect AndroidApi::implWindowClientRect(Window* w) {
  auto* wnd = reinterpret_cast<AndroidWindow*>(w);
  if (wnd == nullptr)
    return Rect(0, 0, 0, 0);
  return Rect(0, 0, wnd->width, wnd->height);
}

bool AndroidApi::implSetAsFullscreen(Window* w, bool fullScreen) {
  auto* wnd = reinterpret_cast<AndroidWindow*>(w);
  if (wnd != nullptr) {
    wnd->isFullscreen = fullScreen;
  }
  // Android apps are typically always fullscreen
  return true;
}

bool AndroidApi::implIsFullscreen(Window* w) {
  auto* wnd = reinterpret_cast<AndroidWindow*>(w);
  return wnd != nullptr ? wnd->isFullscreen : true;
}

void AndroidApi::implSetCursorPosition(Window* w, int x, int y) {
  // No cursor on Android touch devices
}

void AndroidApi::implShowCursor(Window* w, CursorShape cursor) {
  // No cursor on Android touch devices
}

bool AndroidApi::implIsRunning() {
  return g_isRunning.load();
}

int AndroidApi::implExec(AppCallBack& cb) {
  g_isRunning.store(true);

  while (g_isRunning.load()) {
    // Process Android events
    int events;
    struct android_poll_source* source;

    // Poll with timeout when active, block when inactive
    int timeout = g_isActive.load() ? 0 : -1;
    while (ALooper_pollOnce(timeout, nullptr, &events, reinterpret_cast<void**>(&source)) >= 0) {
      if (source != nullptr) {
        source->process(g_app, source);
      }

      if (g_app->destroyRequested != 0) {
        g_isRunning.store(false);
        break;
      }

      timeout = 0; // Don't block on subsequent polls in this iteration
    }

    if (!g_isRunning.load())
      break;

    // Process our event queue
    implProcessEvents(cb);

    // Run the timer callback (game loop)
    if (g_isActive.load() && g_hasWindow.load()) {
      if (!cb.onTimer()) {
        std::this_thread::yield();
      }
    }
  }

  return 0;
}

void AndroidApi::implProcessEvents(AppCallBack& cb) {
  if (g_mainWindow == nullptr || g_mainWindow->owner == nullptr)
    return;

  auto& wnd = *g_mainWindow->owner;
  AppEvent evt;

  while (popEvent(evt)) {
    switch (evt.type) {
      case AppEvent::Resize: {
        SizeEvent e(evt.data.resize.w, evt.data.resize.h);
        AndroidApi::dispatchResize(wnd, e);
        break;
      }

      case AppEvent::TouchDown: {
        MouseEvent e(evt.data.touch.x, evt.data.touch.y,
                     Event::ButtonLeft, Event::M_NoModifier,
                     0, evt.data.touch.pointerId, Event::MouseDown);
        AndroidApi::dispatchMouseDown(wnd, e);
        break;
      }

      case AppEvent::TouchMove: {
        MouseEvent e(evt.data.touch.x, evt.data.touch.y,
                     Event::ButtonLeft, Event::M_NoModifier,
                     0, evt.data.touch.pointerId, Event::MouseMove);
        AndroidApi::dispatchMouseMove(wnd, e);
        break;
      }

      case AppEvent::TouchUp: {
        MouseEvent e(evt.data.touch.x, evt.data.touch.y,
                     Event::ButtonLeft, Event::M_NoModifier,
                     0, evt.data.touch.pointerId, Event::MouseUp);
        AndroidApi::dispatchMouseUp(wnd, e);
        break;
      }

      case AppEvent::KeyDown: {
        // Map Android key codes to Tempest key codes
        KeyEvent e(static_cast<Event::KeyType>(evt.data.key.keyCode),
                   evt.data.key.keyCode, Event::M_NoModifier, Event::KeyDown);
        AndroidApi::dispatchKeyDown(wnd, e, evt.data.key.keyCode);
        break;
      }

      case AppEvent::KeyUp: {
        KeyEvent e(static_cast<Event::KeyType>(evt.data.key.keyCode),
                   evt.data.key.keyCode, Event::M_NoModifier, Event::KeyUp);
        AndroidApi::dispatchKeyUp(wnd, e, evt.data.key.keyCode);
        break;
      }

      case AppEvent::Focus: {
        FocusEvent e(evt.data.focus.gained, Event::FocusReason::UnknownReason);
        AndroidApi::dispatchFocus(wnd, e);
        break;
      }

      case AppEvent::Close: {
        CloseEvent e;
        AndroidApi::dispatchClose(wnd, e);
        break;
      }

      default:
        break;
    }
  }

  // Trigger render if active
  if (g_isActive.load() && g_hasWindow.load() && g_mainWindow->hasPendingFrame.load()) {
    g_mainWindow->hasPendingFrame.store(false);
    AndroidApi::dispatchRender(wnd);
    g_mainWindow->hasPendingFrame.store(true);
  }
}

void AndroidApi::implSetWindowTitle(Window* w, const char* utf8) {
  // Android doesn't have traditional window titles
}

// Entry point called from android_main
extern "C" void tempest_android_main(struct android_app* app);

void tempest_android_main(struct android_app* app) {
  g_app = app;
  app->onAppCmd     = onAppCmd;
  app->onInputEvent = onInputEvent;

  // Wait for window to be ready
  while (!g_hasWindow.load()) {
    int events;
    struct android_poll_source* source;
    if (ALooper_pollOnce(-1, nullptr, &events, reinterpret_cast<void**>(&source)) >= 0) {
      if (source != nullptr) {
        source->process(app, source);
      }
    }
    if (app->destroyRequested != 0) {
      return;
    }
  }
}

// Provide access to native window for Vulkan surface creation
extern "C" ANativeWindow* tempest_android_get_native_window() {
  return g_mainWindow != nullptr ? g_mainWindow->nativeWindow : nullptr;
}

extern "C" struct android_app* tempest_android_get_app() {
  return g_app;
}

#endif // __ANDROID__
