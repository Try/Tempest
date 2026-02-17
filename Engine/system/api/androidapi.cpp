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
#include <android/keycodes.h>
#include <jni.h>

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

// Gamepad state tracking (uses struct from SystemApi)
static GamepadState g_gamepad;

GamepadState AndroidApi::implGamepadState() {
  return g_gamepad;
}

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
    Close,
    GamepadAxis
  };
  Type type = None;

  union {
    struct { int32_t w, h; } resize;
    struct { int x, y, pointerId; } touch;
    struct { uint32_t keyCode; } key;
    struct { bool gained; } focus;
    struct { float lx, ly, rx, ry, lt, rt; } gamepad;
  } data;
};

static std::mutex              g_eventMutex;
static std::queue<AppEvent>    g_eventQueue;

// Enable immersive fullscreen mode (hide navigation bar)
static void enableImmersiveMode() {
  if (g_app == nullptr || g_app->activity == nullptr)
    return;

  JNIEnv* env = nullptr;
  g_app->activity->vm->AttachCurrentThread(&env, nullptr);
  if (env == nullptr)
    return;

  jclass activityClass = env->GetObjectClass(g_app->activity->clazz);
  if (activityClass == nullptr) {
    g_app->activity->vm->DetachCurrentThread();
    return;
  }

  // Get the window
  jmethodID getWindow = env->GetMethodID(activityClass, "getWindow", "()Landroid/view/Window;");
  if (getWindow == nullptr) {
    g_app->activity->vm->DetachCurrentThread();
    return;
  }
  jobject window = env->CallObjectMethod(g_app->activity->clazz, getWindow);
  if (window == nullptr) {
    g_app->activity->vm->DetachCurrentThread();
    return;
  }

  // Get the decor view
  jclass windowClass = env->GetObjectClass(window);
  jmethodID getDecorView = env->GetMethodID(windowClass, "getDecorView", "()Landroid/view/View;");
  if (getDecorView == nullptr) {
    g_app->activity->vm->DetachCurrentThread();
    return;
  }
  jobject decorView = env->CallObjectMethod(window, getDecorView);
  if (decorView == nullptr) {
    g_app->activity->vm->DetachCurrentThread();
    return;
  }

  // Set system UI visibility flags for immersive mode
  jclass viewClass = env->GetObjectClass(decorView);
  jmethodID setSystemUiVisibility = env->GetMethodID(viewClass, "setSystemUiVisibility", "(I)V");
  if (setSystemUiVisibility != nullptr) {
    // SYSTEM_UI_FLAG_FULLSCREEN = 0x00000004
    // SYSTEM_UI_FLAG_HIDE_NAVIGATION = 0x00000002
    // SYSTEM_UI_FLAG_IMMERSIVE_STICKY = 0x00001000
    // SYSTEM_UI_FLAG_LAYOUT_STABLE = 0x00000100
    // SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION = 0x00000200
    // SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN = 0x00000400
    const int flags = 0x00000004 | 0x00000002 | 0x00001000 | 0x00000100 | 0x00000200 | 0x00000400;
    env->CallVoidMethod(decorView, setSystemUiVisibility, flags);
    LOGI("Immersive mode enabled");
  }

  g_app->activity->vm->DetachCurrentThread();
}

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
        enableImmersiveMode();  // Enable immersive fullscreen mode
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
      enableImmersiveMode();  // Re-enable immersive mode when focus is gained
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
      enableImmersiveMode();  // Re-enable immersive mode on resume
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
  int32_t source = AInputEvent_getSource(event);

  if (eventType == AINPUT_EVENT_TYPE_MOTION) {
    // Check if this is gamepad/joystick input
    if ((source & AINPUT_SOURCE_JOYSTICK) == AINPUT_SOURCE_JOYSTICK ||
        (source & AINPUT_SOURCE_GAMEPAD) == AINPUT_SOURCE_GAMEPAD) {
      // Read analog stick values
      float lx = AMotionEvent_getAxisValue(event, AMOTION_EVENT_AXIS_X, 0);
      float ly = AMotionEvent_getAxisValue(event, AMOTION_EVENT_AXIS_Y, 0);
      float rx = AMotionEvent_getAxisValue(event, AMOTION_EVENT_AXIS_Z, 0);
      float ry = AMotionEvent_getAxisValue(event, AMOTION_EVENT_AXIS_RZ, 0);
      float lt = AMotionEvent_getAxisValue(event, AMOTION_EVENT_AXIS_LTRIGGER, 0);
      float rt = AMotionEvent_getAxisValue(event, AMOTION_EVENT_AXIS_RTRIGGER, 0);

      // Some controllers use HAT axes for D-pad
      float hatX = AMotionEvent_getAxisValue(event, AMOTION_EVENT_AXIS_HAT_X, 0);
      float hatY = AMotionEvent_getAxisValue(event, AMOTION_EVENT_AXIS_HAT_Y, 0);

      // Apply deadzone (0.15)
      auto applyDeadzone = [](float v) -> float {
        const float deadzone = 0.15f;
        if (std::abs(v) < deadzone) return 0.0f;
        return v;
      };

      lx = applyDeadzone(lx);
      ly = applyDeadzone(ly);
      rx = applyDeadzone(rx);
      ry = applyDeadzone(ry);

      // Update global state
      g_gamepad.leftStickX  = lx;
      g_gamepad.leftStickY  = ly;
      g_gamepad.rightStickX = rx;
      g_gamepad.rightStickY = ry;
      g_gamepad.leftTrigger  = lt;
      g_gamepad.rightTrigger = rt;
      g_gamepad.connected   = true;

      // Push gamepad axis event
      AppEvent evt;
      evt.type = AppEvent::GamepadAxis;
      evt.data.gamepad.lx = lx;
      evt.data.gamepad.ly = ly;
      evt.data.gamepad.rx = rx;
      evt.data.gamepad.ry = ry;
      evt.data.gamepad.lt = lt;
      evt.data.gamepad.rt = rt;
      pushEvent(evt);

      // Generate D-pad key events from HAT
      static float lastHatX = 0, lastHatY = 0;
      if (hatX != lastHatX) {
        if (lastHatX < -0.5f) { AppEvent e; e.type = AppEvent::KeyUp; e.data.key.keyCode = AKEYCODE_DPAD_LEFT; pushEvent(e); }
        if (lastHatX > 0.5f)  { AppEvent e; e.type = AppEvent::KeyUp; e.data.key.keyCode = AKEYCODE_DPAD_RIGHT; pushEvent(e); }
        if (hatX < -0.5f) { AppEvent e; e.type = AppEvent::KeyDown; e.data.key.keyCode = AKEYCODE_DPAD_LEFT; pushEvent(e); }
        if (hatX > 0.5f)  { AppEvent e; e.type = AppEvent::KeyDown; e.data.key.keyCode = AKEYCODE_DPAD_RIGHT; pushEvent(e); }
        lastHatX = hatX;
      }
      if (hatY != lastHatY) {
        if (lastHatY < -0.5f) { AppEvent e; e.type = AppEvent::KeyUp; e.data.key.keyCode = AKEYCODE_DPAD_UP; pushEvent(e); }
        if (lastHatY > 0.5f)  { AppEvent e; e.type = AppEvent::KeyUp; e.data.key.keyCode = AKEYCODE_DPAD_DOWN; pushEvent(e); }
        if (hatY < -0.5f) { AppEvent e; e.type = AppEvent::KeyDown; e.data.key.keyCode = AKEYCODE_DPAD_UP; pushEvent(e); }
        if (hatY > 0.5f)  { AppEvent e; e.type = AppEvent::KeyDown; e.data.key.keyCode = AKEYCODE_DPAD_DOWN; pushEvent(e); }
        lastHatY = hatY;
      }

      return 1;
    }

    // Touch input
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

    // Mark gamepad as connected if we get gamepad button input
    if ((source & AINPUT_SOURCE_GAMEPAD) == AINPUT_SOURCE_GAMEPAD) {
      g_gamepad.connected = true;
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
  // Setup key translation table for Android keycodes
  static const TranslateKeyPair k[] = {
    { AKEYCODE_CTRL_LEFT,    Event::K_LControl },
    { AKEYCODE_CTRL_RIGHT,   Event::K_RControl },

    { AKEYCODE_SHIFT_LEFT,   Event::K_LShift   },
    { AKEYCODE_SHIFT_RIGHT,  Event::K_RShift   },

    { AKEYCODE_ALT_LEFT,     Event::K_LAlt     },
    { AKEYCODE_ALT_RIGHT,    Event::K_RAlt     },

    { AKEYCODE_DPAD_LEFT,    Event::K_Left     },
    { AKEYCODE_DPAD_RIGHT,   Event::K_Right    },
    { AKEYCODE_DPAD_UP,      Event::K_Up       },
    { AKEYCODE_DPAD_DOWN,    Event::K_Down     },

    { AKEYCODE_ESCAPE,       Event::K_ESCAPE   },
    { AKEYCODE_BACK,         Event::K_ESCAPE   },  // Android back button as escape
    { AKEYCODE_DEL,          Event::K_Back     },  // Android DEL is backspace
    { AKEYCODE_TAB,          Event::K_Tab      },
    { AKEYCODE_FORWARD_DEL,  Event::K_Delete   },
    { AKEYCODE_INSERT,       Event::K_Insert   },
    { AKEYCODE_MOVE_HOME,    Event::K_Home     },
    { AKEYCODE_MOVE_END,     Event::K_End      },
    { AKEYCODE_BREAK,        Event::K_Pause    },
    { AKEYCODE_ENTER,        Event::K_Return   },
    { AKEYCODE_SPACE,        Event::K_Space    },
    { AKEYCODE_CAPS_LOCK,    Event::K_CapsLock },

    // Gamepad buttons
    { AKEYCODE_BUTTON_A,      Event::K_Return   },  // A = confirm/action
    { AKEYCODE_BUTTON_B,      Event::K_ESCAPE   },  // B = back/cancel
    { AKEYCODE_BUTTON_X,      Event::K_Space    },  // X = jump
    { AKEYCODE_BUTTON_Y,      Event::K_Tab      },  // Y = inventory
    { AKEYCODE_BUTTON_L1,     Event::K_Q        },  // L1 = prev weapon
    { AKEYCODE_BUTTON_R1,     Event::K_E        },  // R1 = next weapon
    { AKEYCODE_BUTTON_START,  Event::K_ESCAPE   },  // Start = menu
    { AKEYCODE_BUTTON_SELECT, Event::K_Tab      },  // Select = inventory

    { AKEYCODE_F1,           Event::K_F1       },
    { AKEYCODE_0,            Event::K_0        },
    { AKEYCODE_A,            Event::K_A        },

    { 0,                     Event::K_NoKey    }
    };
  setupKeyTranslate(k, 32);
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
        // Map Android key codes to Tempest key codes using the translation table
        auto key = Event::KeyType(translateKey(evt.data.key.keyCode));
        KeyEvent e(key, evt.data.key.keyCode, Event::M_NoModifier, Event::KeyDown);
        AndroidApi::dispatchKeyDown(wnd, e, evt.data.key.keyCode);
        break;
      }

      case AppEvent::KeyUp: {
        auto key = Event::KeyType(translateKey(evt.data.key.keyCode));
        KeyEvent e(key, evt.data.key.keyCode, Event::M_NoModifier, Event::KeyUp);
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

// The application entry point - calls user's main() after setup
int main(int argc, const char** argv);

extern "C" void android_main(struct android_app* app) {
  tempest_android_main(app);

  const char* argv[] = {"app", nullptr};
  main(1, argv);
}

#endif // __ANDROID__
