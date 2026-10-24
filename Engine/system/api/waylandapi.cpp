#include "waylandapi.h"

#if defined(TEMPEST_BUILD_WAYLAND)
#include <Tempest/Event>
#include <Tempest/Except>
#include <Tempest/Window>
#include <Tempest/Log>

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <memory>
#include <thread>
#include <vector>

#include <linux/input-event-codes.h>
#include <poll.h>
#include <sys/mman.h>
#include <unistd.h>

#include <wayland-client.h>
#include <xkbcommon/xkbcommon.h>

#include "xdg-shell-client-protocol.h"
#include "xdg-decoration-unstable-v1-client-protocol.h"
#include "viewporter-client-protocol.h"
#include "fractional-scale-v1-client-protocol.h"
#include "cursor-shape-v1-client-protocol.h"
#include "pointer-constraints-unstable-v1-client-protocol.h"
#include "relative-pointer-unstable-v1-client-protocol.h"

using namespace Tempest;

static wl_display*      dpy = nullptr; // non-null while Wayland is the active backend, written only by constructor/destructor
static std::atomic_bool isExit{0};

////
///   WINDOW STATE
//

// Per-window state. user data in SystemApi::Window*/wl_surface* handed to Tempest
struct WWindow {
  Tempest::Window*             owner           = nullptr;

  wl_surface*                  surface         = nullptr;
  xdg_surface*                 xdgSurface      = nullptr;
  xdg_toplevel*                toplevel        = nullptr;
  zxdg_toplevel_decoration_v1* decoration      = nullptr;
  wp_viewport*                 viewport        = nullptr;
  wp_fractional_scale_v1*      fractionalScale = nullptr;

  // Announced by xdg_toplevel.configure, becomes current on xdg_surface.configure
  struct Pending {
    int32_t width      = 0; // logical; 0 = no size from the compositor yet
    int32_t height     = 0;
    bool    fullscreen = false;
    bool    suspended  = false;
    } pending;

  // Current state
  int32_t  width          = 0;     // logical
  int32_t  height         = 0;
  uint32_t preferredScale = 120;   // wp_fractional_scale_v1: scale * 120
  bool     fullscreen     = false;
  bool     suspended      = false; // compositor doesn't repaint the window (e.g. minimized): pause rendering
  bool     configured     = false; // first configure acked; no buffer (= Vulkan present) before that

  // Pointer lock for mouse-look, requested while the window's cursor is hidden (see Private::setPointerLock).
  zwp_locked_pointer_v1* lockedPointer = nullptr;
  bool                   pointerLocked = false;

  // Pointer position as the compositor last reported it (enter, motion). Recorded into MouseMove events.
  double                 compositorPointerX = 0; // physical
  double                 compositorPointerY = 0;

  // Cursor as the app has seen it: set when a MouseMove is dispatched (or by setCursorPosition while locked), read
  // for buttons and wheel, which carry no position. While the pointer is locked, a virtual cursor moved by relative motion.
  double                 appCursorX         = 0; // physical
  double                 appCursorY         = 0;

  bool    readyToRender() const  { return owner!=nullptr && configured && !suspended; }

  float   scale() const          { return float(preferredScale)/120.f; }
  // Physical size, as Tempest sees it (client rect, resize events, swapchain extent). Rounded half up as fractional-scale
  // requires. In integers, since scales like 1.15 aren't exact as float.
  int32_t physicalWidth() const  { return int32_t((int64_t(width) *preferredScale + 60)/120); }
  int32_t physicalHeight() const { return int32_t((int64_t(height)*preferredScale + 60)/120); }
  };

static WWindow* toWWindow(SystemApi::Window* w) {
  // the wl_surface listener's data (= its user data) is the WWindow, see createWindow()
  if(w==nullptr)
    return nullptr;
  return static_cast<WWindow*>(wl_surface_get_user_data(reinterpret_cast<wl_surface*>(w)));
  }

////
///   APP EVENTS
//

// Events for the app/systemAPI. Listeners only record them. implProcessEvents() dispatches one per call
struct AppEvent {
  Tempest::Event::Type        type;
  WWindow*                    w;
  // Data per type, recorded by the listener
  Tempest::Event::KeyType     key     = Tempest::Event::K_NoKey;    // KeyDown/KeyUp
  uint32_t                    code    = 0;                          // KeyDown/KeyUp: typed character
  uint32_t                    keycode = 0;                          // KeyDown/KeyUp: scancode for the dispatcher
  Tempest::Event::MouseButton button  = Tempest::Event::ButtonNone; // MouseDown/MouseUp
  int                         delta   = 0;                          // MouseWheel
  double                      x       = 0;                          // MouseMove: physical position
  double                      y       = 0;
  bool                        focusIn = false;                      // Focus
  };

////
///   SESSION STATE
//

struct WaylandApi::Private {
  wl_display*                      display                = nullptr;
  wl_registry*                     registry               = nullptr;

  // Globals
  wl_compositor*                   compositor             = nullptr;
  wl_seat*                         seat                   = nullptr;
  xdg_wm_base*                     wmBase                 = nullptr;
  zxdg_decoration_manager_v1*      decorationManager      = nullptr;
  wp_viewporter*                   viewporter             = nullptr;
  wp_fractional_scale_manager_v1*  fractionalScaleManager = nullptr;
  wp_cursor_shape_manager_v1*      cursorShapeManager     = nullptr;
  zwp_pointer_constraints_v1*      pointerConstraints     = nullptr;
  zwp_relative_pointer_manager_v1* relativePointerManager = nullptr;

  // Exact versions we need to bind successfully: listeners and requests are written for these
  static constexpr uint32_t wlCompositorVersion                = 1;
  static constexpr uint32_t wlSeatVersion                      = 8; // v8: wl_pointer.axis_value120 (wheel in 1/120 notches)
  static constexpr uint32_t xdgWmBaseVersion                   = 6; // v6: xdg_toplevel state 'suspended'
  static constexpr uint32_t zxdgDecorationManagerV1Version     = 1;
  static constexpr uint32_t wpViewporterVersion                = 1;
  static constexpr uint32_t wpFractionalScaleManagerV1Version  = 1;
  static constexpr uint32_t wpCursorShapeManagerV1Version      = 1;
  static constexpr uint32_t zwpPointerConstraintsV1Version     = 1;
  static constexpr uint32_t zwpRelativePointerManagerV1Version = 1;

  // Input devices, non-null while seat has the capability (can change at runtime)
  wl_pointer*                     pointer                = nullptr;
  wp_cursor_shape_device_v1*      cursorShapeDevice      = nullptr; // exists together with 'pointer'
  zwp_relative_pointer_v1*        relativePointer        = nullptr; // exists together with 'pointer'
  wl_keyboard*                    keyboard               = nullptr;

  // Pointer state between wl_pointer.enter and leave. Position is in WWindow::compositorPointerX/Y
  WWindow*                        pointerFocus           = nullptr; // window under pointer
  uint32_t                        pointerEnterSerial     = 0;       // serial of latest enter event

  // Keymap from wl_keyboard.keymap, state follows wl_keyboard.modifiers; both null until first keymap.
  xkb_context*                    xkbContext             = nullptr;
  xkb_keymap*                     xkbKeymap              = nullptr;
  xkb_state*                      xkbState               = nullptr;
  WWindow*                        keyboardFocus          = nullptr; // window with keyboard focus (wl_keyboard.enter/leave)

  // Keys (xkb keycodes) and buttons (evdev codes) whose press reached the app. Only their releases are dispatched,
  // and they're released on focus or device loss (see releaseHeldKeys / releaseHeldButtons).
  std::vector<uint32_t>           heldKeys;
  std::vector<uint32_t>           heldButtons;

  // Key repeat: Wayland sends none. Manual implementation from wl_keyboard.repeat_info by sending the held key's KeyDown again,
  // which Tempest's dispatcher turns into KeyRepeat (like on Windows).
  int32_t                         repeatRate             = 0;       // repeats per second; 0 = disabled
  int32_t                         repeatDelay            = 0;       // ms until the first repeat trigger
  uint32_t                        repeatKeycode          = 0;       // xkb keycode of the repeating key; 0 = none.
  // Non-zero only while keyboardFocus is set and repeatRate>0.
  std::chrono::steady_clock::time_point repeatNext;                 // when the next repeat is due

  // WWindow* if a window gained focus during active readEvents() batch, otherwise nullptr (for focus click filter)
  WWindow*                        focusGainedInBatch     = nullptr;

  std::vector<std::unique_ptr<WWindow>> windows; // owns the windows; each WWindow keeps its address (handle, listener data)
  static inline std::deque<AppEvent>    appEvents; // in the order the compositor sent them

  // Session listeners (globals and seat devices). 'data' is Private*.
  static void onWlRegistryGlobal(void* data, wl_registry* registry, uint32_t name, const char* interface, uint32_t version);
  static void onXdgWmBasePing(void* data, xdg_wm_base* wmBase, uint32_t serial);
  static void onWlSeatCapabilities(void* data, wl_seat* seat, uint32_t capabilities);
  static void onWlPointerEnter(void* data, wl_pointer* pointer, uint32_t serial, wl_surface* surface, wl_fixed_t x, wl_fixed_t y);
  static void onWlPointerLeave(void* data, wl_pointer* pointer, uint32_t serial, wl_surface* surface);
  static void onWlPointerMotion(void* data, wl_pointer* pointer, uint32_t time, wl_fixed_t x, wl_fixed_t y);
  static void onWlPointerButton(void* data, wl_pointer* pointer, uint32_t serial, uint32_t time, uint32_t button, uint32_t state);
  static void onWlPointerAxisValue120(void* data, wl_pointer* pointer, uint32_t axis, int32_t value120);
  static void onZwpRelativePointerV1RelativeMotion(void* data, zwp_relative_pointer_v1* relativePointer, uint32_t utimeHi,
                                                   uint32_t utimeLo, wl_fixed_t dx, wl_fixed_t dy,
                                                   wl_fixed_t dxUnaccel, wl_fixed_t dyUnaccel);
  static void onWlKeyboardKeymap(void* data, wl_keyboard* keyboard, uint32_t format, int32_t fd, uint32_t size);
  static void onWlKeyboardEnter(void* data, wl_keyboard* keyboard, uint32_t serial, wl_surface* surface, wl_array* keys);
  static void onWlKeyboardLeave(void* data, wl_keyboard* keyboard, uint32_t serial, wl_surface* surface);
  static void onWlKeyboardKey(void* data, wl_keyboard* keyboard, uint32_t serial, uint32_t time, uint32_t key, uint32_t state);
  static void onWlKeyboardModifiers(void* data, wl_keyboard* keyboard, uint32_t serial, uint32_t depressed,
                                    uint32_t latched, uint32_t locked, uint32_t group);
  static void onWlKeyboardRepeatInfo(void* data, wl_keyboard* keyboard, int32_t rate, int32_t delay);

  // Window specific listeners. 'data' is WWindow*.
  static void onXdgSurfaceConfigure(void* data, xdg_surface* xdgSurface, uint32_t serial);
  static void onXdgToplevelConfigure(void* data, xdg_toplevel* toplevel, int32_t width, int32_t height, wl_array* states);
  static void onXdgToplevelClose    (void* data, xdg_toplevel* toplevel);
  static void onZxdgToplevelDecorationV1Configure(void* data, zxdg_toplevel_decoration_v1* decoration, uint32_t mode);
  static void onWpFractionalScaleV1PreferredScale(void* data, wp_fractional_scale_v1* fractionalScale, uint32_t scale);
  static void onZwpLockedPointerV1Locked  (void* data, zwp_locked_pointer_v1* lockedPointer);
  static void onZwpLockedPointerV1Unlocked(void* data, zwp_locked_pointer_v1* lockedPointer);

  // Listener tables (session)
  static const wl_registry_listener                 wlRegistryListener;
  static const xdg_wm_base_listener                 xdgWmBaseListener;
  static const wl_seat_listener                     wlSeatListener;
  static const wl_pointer_listener                  wlPointerListener;
  static const zwp_relative_pointer_v1_listener     zwpRelativePointerV1Listener;
  static const wl_keyboard_listener                 wlKeyboardListener;

  // Listener tables (window specific)
  static const wl_surface_listener                  wlSurfaceListener;
  static const xdg_surface_listener                 xdgSurfaceListener;
  static const xdg_toplevel_listener                xdgToplevelListener;
  static const zxdg_toplevel_decoration_v1_listener zxdgToplevelDecorationV1Listener;
  static const wp_fractional_scale_v1_listener      wpFractionalScaleV1Listener;
  static const zwp_locked_pointer_v1_listener       zwpLockedPointerV1Listener;

  // Helpers

  static constexpr auto                  ioIgnore = [](auto...) {}; // useful to silence Wayland listener stubs
  template<class T> static void          ioDestroy(T*& obj, void (*destroyFn)(T*));
  void                                   logDisplayError() const;
  static Event::MouseButton              toButton(uint32_t button);
  static wp_cursor_shape_device_v1_shape toCursorShape(CursorShape shape);
  static MouseEvent                      pointerEvent(const WWindow& w, Event::MouseButton button, int delta, Event::Type type);
  AppEvent                               keyEvent(WWindow& w, uint32_t keycode, Event::Type type) const;
  bool                                   isWindowFocused(const WWindow* w) const;
  void                                   setPointerLock(WWindow& w, bool lock);
  void                                   releaseHeldKeys(WWindow& w);
  void                                   releaseHeldButtons(WWindow& w);
  void                                   releasePointer();
  void                                   releaseKeyboard();
  void                                   startKeyRepeat(uint32_t keycode);
  void                                   processKeyRepeat();
  static void                            dispatchCloseEvent(WWindow& w);
  static void                            dispatchKeyEvent(const AppEvent& e);
  static void                            dispatchFocusEvent(const AppEvent& e);
  static void                            dispatchMouseEvent(const AppEvent& e);

  // Connection and windows
  bool               hasRequiredGlobals() const;
  bool               connect();
                     ~Private();
  SystemApi::Window* createWindow(Tempest::Window* owner, uint32_t width, uint32_t height, SystemApi::ShowMode sm);
  void               destroyWindow(WWindow* w);

  // Event loop
  void        handleConnectionError();
  int         pollTimeout(const SystemApi::AppCallBack& cb) const;
  void        readEvents(const SystemApi::AppCallBack& cb);
  static bool dispatchAppEvent();
  static void updateState(WWindow& w);
  void        renderWindows();
  };

////
///     LISTENERS
//

///     Session listeners
//
void WaylandApi::Private::onWlRegistryGlobal(void* data, wl_registry* registry, uint32_t name, const char* interface, uint32_t version) {
  // Binds exactly the *Version constants; older globals are skipped.
  WaylandApi::Private* self = static_cast<Private*>(data);
  if(std::strcmp(interface, wl_compositor_interface.name)==0 && version>=wlCompositorVersion) {
    self->compositor = static_cast<wl_compositor*>(wl_registry_bind(registry, name, &wl_compositor_interface, wlCompositorVersion));
    }
  if(std::strcmp(interface, xdg_wm_base_interface.name)==0 && version>=xdgWmBaseVersion) {
    self->wmBase = static_cast<xdg_wm_base*>(wl_registry_bind(registry, name, &xdg_wm_base_interface, xdgWmBaseVersion));
    xdg_wm_base_add_listener(self->wmBase, &xdgWmBaseListener, self);
    }
  if(std::strcmp(interface, zxdg_decoration_manager_v1_interface.name)==0 && version>=zxdgDecorationManagerV1Version) {
    self->decorationManager = static_cast<zxdg_decoration_manager_v1*>(
        wl_registry_bind(registry, name, &zxdg_decoration_manager_v1_interface, zxdgDecorationManagerV1Version));
    }
  if(std::strcmp(interface, wp_viewporter_interface.name)==0 && version>=wpViewporterVersion) {
    self->viewporter = static_cast<wp_viewporter*>(wl_registry_bind(registry, name, &wp_viewporter_interface, wpViewporterVersion));
    }
  if(std::strcmp(interface, wp_fractional_scale_manager_v1_interface.name)==0 && version>=wpFractionalScaleManagerV1Version) {
    self->fractionalScaleManager = static_cast<wp_fractional_scale_manager_v1*>(
        wl_registry_bind(registry, name, &wp_fractional_scale_manager_v1_interface, wpFractionalScaleManagerV1Version));
    }
  if(std::strcmp(interface, wp_cursor_shape_manager_v1_interface.name)==0 && version>=wpCursorShapeManagerV1Version) {
    self->cursorShapeManager = static_cast<wp_cursor_shape_manager_v1*>(
        wl_registry_bind(registry, name, &wp_cursor_shape_manager_v1_interface, wpCursorShapeManagerV1Version));
    }
  if(std::strcmp(interface, zwp_pointer_constraints_v1_interface.name)==0 && version>=zwpPointerConstraintsV1Version) {
    self->pointerConstraints = static_cast<zwp_pointer_constraints_v1*>(
        wl_registry_bind(registry, name, &zwp_pointer_constraints_v1_interface, zwpPointerConstraintsV1Version));
    }
  if(std::strcmp(interface, zwp_relative_pointer_manager_v1_interface.name)==0 && version>=zwpRelativePointerManagerV1Version) {
    self->relativePointerManager = static_cast<zwp_relative_pointer_manager_v1*>(
        wl_registry_bind(registry, name, &zwp_relative_pointer_manager_v1_interface, zwpRelativePointerManagerV1Version));
    }
  if(std::strcmp(interface, wl_seat_interface.name)==0 && version>=wlSeatVersion && self->seat==nullptr) {
    // Only the first seat: multi-seat setups are rare, and Tempest has a single pointer/keyboard.
    static_assert(wlSeatVersion<WL_POINTER_AXIS_RELATIVE_DIRECTION_SINCE_VERSION,
                  "wlPointerListener needs an axis_relative_direction handler at this version");
    self->seat = static_cast<wl_seat*>(wl_registry_bind(registry, name, &wl_seat_interface, wlSeatVersion));
    wl_seat_add_listener(self->seat, &wlSeatListener, self);
    }
  }

void WaylandApi::Private::onXdgWmBasePing([[maybe_unused]] void* data, xdg_wm_base* wmBase, uint32_t serial) {
  xdg_wm_base_pong(wmBase, serial);
  }

void WaylandApi::Private::onWlSeatCapabilities(void* data, wl_seat* seat, uint32_t capabilities) {
  // Sent after binding and whenever devices come or go: get a device when its capability appears, release it when it goes.
  WaylandApi::Private* self = static_cast<Private*>(data);

  // The managers are bound: capabilities arrive after connect()'s roundtrip. Pointer locks follow on the next cursor
  // change (always on enter).
  const bool hasPointer = (capabilities & WL_SEAT_CAPABILITY_POINTER)!=0;
  if(hasPointer && self->pointer==nullptr) {
    self->pointer = wl_seat_get_pointer(seat);
    wl_pointer_add_listener(self->pointer, &wlPointerListener, self);
    self->cursorShapeDevice = wp_cursor_shape_manager_v1_get_pointer(self->cursorShapeManager, self->pointer);
    self->relativePointer   = zwp_relative_pointer_manager_v1_get_relative_pointer(self->relativePointerManager, self->pointer);
    zwp_relative_pointer_v1_add_listener(self->relativePointer, &zwpRelativePointerV1Listener, self);
    }
  else if(!hasPointer && self->pointer!=nullptr) {
    self->releasePointer();
    }

  const bool hasKeyboard = (capabilities & WL_SEAT_CAPABILITY_KEYBOARD)!=0;
  if(hasKeyboard && self->keyboard==nullptr) {
    self->keyboard = wl_seat_get_keyboard(seat);
    wl_keyboard_add_listener(self->keyboard, &wlKeyboardListener, self);
    }
  else if(!hasKeyboard && self->keyboard!=nullptr) {
    self->releaseKeyboard();
    }
  }

void WaylandApi::Private::onWlPointerEnter(void* data, [[maybe_unused]] wl_pointer* pointer, uint32_t serial,
                                           wl_surface* surface, wl_fixed_t x, wl_fixed_t y) {
  // Like X11's EnterNotify: a move to the entry position, then re-apply the window's cursor
  auto self = static_cast<Private*>(data);
  if(surface==nullptr)
    return; // surface was destroyed after compositor sent the event
  auto w = static_cast<WWindow*>(wl_surface_get_user_data(surface)); // listener data of our wl_surface
  self->pointerFocus       = w;
  self->pointerEnterSerial = serial;
  // logical -> physical
  w->compositorPointerX    = wl_fixed_to_double(x)*w->scale();
  w->compositorPointerY    = wl_fixed_to_double(y)*w->scale();
  if(w->owner==nullptr)
    return;
  if(self->isWindowFocused(w))
    appEvents.push_back({.type=Event::MouseMove, .w=w, .x=w->compositorPointerX, .y=w->compositorPointerY});
  // Note: Wayland always requires cursor shape on enter (see implShowCursor)
  SystemApi::showCursor(reinterpret_cast<SystemApi::Window*>(w->surface), SystemApi::cursorShape(*w->owner));
  }

void WaylandApi::Private::onWlPointerLeave(void* data, [[maybe_unused]] wl_pointer* pointer, [[maybe_unused]] uint32_t serial,
                                           [[maybe_unused]] wl_surface* surface) {
  // 'surface' may be null if it was destroyed meanwhile.
  static_cast<Private*>(data)->pointerFocus = nullptr;
  }

void WaylandApi::Private::onWlPointerMotion(void* data, [[maybe_unused]] wl_pointer* pointer, [[maybe_unused]] uint32_t time,
                                            wl_fixed_t x, wl_fixed_t y) {
  // While a button is held, coordinates can be outside the window (implicit grab), like on X11. Not sent while locked.
  auto self = static_cast<Private*>(data);
  WWindow* w = self->pointerFocus;
  if(w==nullptr)
    return;
  w->compositorPointerX = wl_fixed_to_double(x)*w->scale();
  w->compositorPointerY = wl_fixed_to_double(y)*w->scale();
  if(w->owner==nullptr || !self->isWindowFocused(w))
    return;
  appEvents.push_back({.type=Event::MouseMove, .w=w, .x=w->compositorPointerX, .y=w->compositorPointerY});
  }

void WaylandApi::Private::onWlPointerButton(void* data, [[maybe_unused]] wl_pointer* pointer, [[maybe_unused]] uint32_t serial,
                                            [[maybe_unused]] uint32_t time, uint32_t button, uint32_t state) {
  // Unknown buttons become ButtonNone, like X11. Releases only for presses the app got (see heldButtons).
  // heldButtons is updated after recording: if push_back throws, both stay consistent.
  auto self = static_cast<Private*>(data);
  WWindow* w = self->pointerFocus;
  if(w==nullptr || w->owner==nullptr)
    return;

  const bool held = std::find(self->heldButtons.begin(), self->heldButtons.end(), button)!=self->heldButtons.end();
  if(state==WL_POINTER_BUTTON_STATE_PRESSED) {
    if(!self->isWindowFocused(w))
      return;
    // Focus click filter: the click that only focuses the window must not reach the app. KWin sends
    // wl_keyboard.enter and that press together, so a press in the same readEvents() batch as the enter is that click.
    if(self->focusGainedInBatch==w)
      return;
    appEvents.push_back({.type=Event::MouseDown, .w=w, .button=toButton(button)});
    if(!held)
      self->heldButtons.push_back(button);
    } else {
    if(!held)
      return;
    appEvents.push_back({.type=Event::MouseUp, .w=w, .button=toButton(button)});
    std::erase(self->heldButtons, button);
    }
  }

void WaylandApi::Private::onWlPointerAxisValue120(void* data, [[maybe_unused]] wl_pointer* pointer, uint32_t axis, int32_t value120) {
  // 120 per notch, like Tempest's delta on Windows; Wayland's sign is inverted (positive = down).
  // Horizontal scrolling is ignored, like on X11.
  if(axis!=WL_POINTER_AXIS_VERTICAL_SCROLL)
    return;

  auto self = static_cast<Private*>(data);
  WWindow* w = self->pointerFocus;
  if(w==nullptr || w->owner==nullptr || !self->isWindowFocused(w))
    return;
  appEvents.push_back({.type=Event::MouseWheel, .w=w, .delta=-value120});
  }

void WaylandApi::Private::onZwpRelativePointerV1RelativeMotion(void* data, [[maybe_unused]] zwp_relative_pointer_v1* relativePointer,
                                                               [[maybe_unused]] uint32_t utimeHi, [[maybe_unused]] uint32_t utimeLo,
                                                               [[maybe_unused]] wl_fixed_t dx, [[maybe_unused]] wl_fixed_t dy,
                                                               wl_fixed_t dxUnaccel, wl_fixed_t dyUnaccel) {
  // Moves virtual cursor while pointer is locked (otherwise wl_pointer.motion has the position). Raw input deltas
  // clamped to the window like a real cursor.
  // Note: compositor only activates the lock while focused
  auto self = static_cast<Private*>(data);
  WWindow* w = self->pointerFocus;
  if(w==nullptr || !w->pointerLocked || w->owner==nullptr)
    return;
  const double s = w->scale();
  w->appCursorX = std::clamp(w->appCursorX + wl_fixed_to_double(dxUnaccel)*s, 0.0, double(std::max(w->physicalWidth() -1, 0)));
  w->appCursorY = std::clamp(w->appCursorY + wl_fixed_to_double(dyUnaccel)*s, 0.0, double(std::max(w->physicalHeight()-1, 0)));
  MouseEvent e = pointerEvent(*w, Event::ButtonNone, 0, Event::MouseMove);
  SystemApi::dispatchMouseMove(*w->owner, e);
  }

void WaylandApi::Private::onWlKeyboardKeymap(void* data, [[maybe_unused]] wl_keyboard* keyboard,
                                             uint32_t format, int32_t fd, uint32_t size) {
  // Sent on creation and when the keymap changes. On failure the previous keymap stays.
  // Note: need to close fd manually inside function!
  auto self = static_cast<Private*>(data);
  if(format!=WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1) {
    Log::e("WaylandApi: unsupported keymap format ", format);
    close(fd);
    return;
    }

  // MAP_PRIVATE: required since wl_keyboard v7
  void* map = mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0);
  close(fd);
  if(map==MAP_FAILED) {
    Log::e("WaylandApi: can't map the keymap: ", std::strerror(errno));
    return;
    }
  // The text is null-terminated within 'size'; strnlen makes sure we never read beyond the mapping.
  const char* text   = static_cast<const char*>(map);
  xkb_keymap* keymap = xkb_keymap_new_from_buffer(self->xkbContext, text, strnlen(text, size),
                                                  XKB_KEYMAP_FORMAT_TEXT_V1, XKB_KEYMAP_COMPILE_NO_FLAGS);
  munmap(map, size);
  if(keymap==nullptr) {
    Log::e("WaylandApi: can't compile the keymap");
    return;
    }
  xkb_state* state = xkb_state_new(keymap);
  if(state==nullptr) {
    Log::e("WaylandApi: can't create the keyboard state");
    xkb_keymap_unref(keymap);
    return;
    }

  ioDestroy(self->xkbState,  xkb_state_unref);
  ioDestroy(self->xkbKeymap, xkb_keymap_unref);
  self->xkbKeymap     = keymap;
  self->xkbState      = state;
  self->repeatKeycode = 0; // keycodes may mean something else in the new keymap

  const char* layout = xkb_keymap_layout_get_name(keymap, 0);
  Log::i("WaylandApi: keymap loaded, ", xkb_keymap_num_layouts(keymap), " layout(s), first: ", (layout!=nullptr ? layout : "unnamed"));
  }

void WaylandApi::Private::onWlKeyboardEnter(void* data, [[maybe_unused]] wl_keyboard* keyboard, [[maybe_unused]] uint32_t serial,
                                            wl_surface* surface, [[maybe_unused]] wl_array* keys) {
  // Like X11's FocusIn. Keys already held ('keys') are ignored: they aren't in heldKeys, so their releases are dropped.
  auto self = static_cast<Private*>(data);
  if(surface==nullptr)
    return; // the surface was destroyed after the compositor sent the event
  auto w = static_cast<WWindow*>(wl_surface_get_user_data(surface));
  self->keyboardFocus      = w;
  self->focusGainedInBatch = w;
  if(w->owner==nullptr)
    return;
  appEvents.push_back({.type=Event::Focus, .w=w, .focusIn=true});
  // Pointer already over the window: like an enter, a move to its position and the app's cursor.
  if(self->pointerFocus==w) {
    appEvents.push_back({.type=Event::MouseMove, .w=w, .x=w->compositorPointerX, .y=w->compositorPointerY});
    SystemApi::showCursor(reinterpret_cast<SystemApi::Window*>(w->surface), SystemApi::cursorShape(*w->owner));
    }
  }

void WaylandApi::Private::onWlKeyboardLeave(void* data, [[maybe_unused]] wl_keyboard* keyboard, [[maybe_unused]] uint32_t serial,
                                            [[maybe_unused]] wl_surface* surface) {
  // Like X11's FocusOut, but held keys and buttons are released first. 'surface' may be null, so the window comes from
  // keyboardFocus.
  auto self = static_cast<Private*>(data);
  WWindow* w = self->keyboardFocus;
  if(w==nullptr)
    return;
  // On focus loss the real releases never reach the app, so keys and buttons would stay held (e.g. Alt after Alt+Tab).
  self->releaseHeldKeys(*w);
  self->releaseHeldButtons(*w);
  self->keyboardFocus = nullptr;
  if(w->owner==nullptr)
    return;
  appEvents.push_back({.type=Event::Focus, .w=w, .focusIn=false});
  if(self->pointerFocus==w)
    SystemApi::showCursor(reinterpret_cast<SystemApi::Window*>(w->surface), SystemApi::cursorShape(*w->owner));
  }

void WaylandApi::Private::onWlKeyboardKey(void* data, [[maybe_unused]] wl_keyboard* keyboard, [[maybe_unused]] uint32_t serial,
                                          [[maybe_unused]] uint32_t time, uint32_t key, uint32_t state) {
  // xkb keycodes are evdev + 8 (X11's keycodes); also the scancode for the dispatcher.
  auto self = static_cast<Private*>(data);
  if(self->keyboardFocus==nullptr || self->keyboardFocus->owner==nullptr)
    return;
  if(self->xkbState==nullptr)
    return; // no keymap compiled
  // Only releases of keys the app got a press for are dispatched (see heldKeys).
  // heldKeys is updated after recording: if push_back throws, both stay consistent.
  const uint32_t keycode = key + 8;
  const bool     held    = std::find(self->heldKeys.begin(), self->heldKeys.end(), keycode)!=self->heldKeys.end();
  if(state==WL_KEYBOARD_KEY_STATE_PRESSED) {
    appEvents.push_back(self->keyEvent(*self->keyboardFocus, keycode, Event::KeyDown));
    if(!held)
      self->heldKeys.push_back(keycode);
    self->startKeyRepeat(keycode);
    } else {
    if(!held)
      return;
    appEvents.push_back(self->keyEvent(*self->keyboardFocus, keycode, Event::KeyUp));
    std::erase(self->heldKeys, keycode);
    if(keycode==self->repeatKeycode)
      self->repeatKeycode = 0;
    }
  }

void WaylandApi::Private::onWlKeyboardModifiers(void* data, [[maybe_unused]] wl_keyboard* keyboard, [[maybe_unused]] uint32_t serial,
                                                uint32_t depressed, uint32_t latched, uint32_t locked, uint32_t group) {
  // Not dispatched: Tempest tracks Shift/Ctrl/Alt from the key events itself. xkb needs them for the text of a key
  // (Shift, CapsLock) and for the active layout ('group').
  auto self = static_cast<Private*>(data);
  if(self->xkbState==nullptr)
    return; // no keymap compiled
  xkb_state_update_mask(self->xkbState, depressed, latched, locked, 0, 0, group);
  }

void WaylandApi::Private::onWlKeyboardRepeatInfo(void* data, [[maybe_unused]] wl_keyboard* keyboard, int32_t rate, int32_t delay) {
  // The desktop setting; rate 0 = repeat disabled.
  auto self = static_cast<Private*>(data);
  self->repeatRate  = rate;
  self->repeatDelay = delay;
  if(rate<=0)
    self->repeatKeycode = 0;
  }

///     Window specific listeners
//
void WaylandApi::Private::onXdgSurfaceConfigure(void* data, [[maybe_unused]] xdg_surface* xdgSurface, uint32_t serial) {
  // End of configure sequence (after xdg_toplevel.configure etc.): pending becomes current and is acked
  auto w = static_cast<WWindow*>(data);
  w->width      = w->pending.width;
  w->height     = w->pending.height;
  w->fullscreen = w->pending.fullscreen;
  w->suspended  = w->pending.suspended;

  // Viewport state is double-buffered: it applies with the next commit, which Vulkan does in vkQueuePresent.
  // Size 0 only if the compositor never sent one (first configure): createWindow() picks the size then.
  if(w->width>0 && w->height>0)
    wp_viewport_set_destination(w->viewport, w->width, w->height);

  xdg_surface_ack_configure(w->xdgSurface, serial);
  w->configured = true;
  }

void WaylandApi::Private::onXdgToplevelConfigure(void* data, [[maybe_unused]] xdg_toplevel* toplevel, int32_t width, int32_t height, wl_array* states) {
  // Only fills w->pending; it becomes current in onXdgSurfaceConfigure.
  auto w = static_cast<WWindow*>(data);
  // Logical size; 0 = client decides, keep the previous size
  if(width>0 && height>0) {
    w->pending.width  = width;
    w->pending.height = height;
    }
  // 'states': the complete list of xdg_toplevel_state values
  w->pending.fullscreen = false;
  w->pending.suspended  = false;
  auto state = static_cast<const uint32_t*>(states->data);
  for(size_t i=0; i<states->size/sizeof(uint32_t); ++i) {
    if(state[i]==XDG_TOPLEVEL_STATE_FULLSCREEN)
      w->pending.fullscreen = true;
    static_assert(xdgWmBaseVersion>=XDG_TOPLEVEL_STATE_SUSPENDED_SINCE_VERSION);
    if(state[i]==XDG_TOPLEVEL_STATE_SUSPENDED)
      w->pending.suspended = true;
    }
  }

void WaylandApi::Private::onXdgToplevelClose(void* data, [[maybe_unused]] xdg_toplevel* toplevel) {
  // Only recorded, see dispatchCloseEvent().
  auto w = static_cast<WWindow*>(data);
  appEvents.push_back({Event::Close, w});
  }

void WaylandApi::Private::onZxdgToplevelDecorationV1Configure([[maybe_unused]] void* data, [[maybe_unused]] zxdg_toplevel_decoration_v1* decoration, uint32_t mode) {
  if(mode==ZXDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE)
    Log::i("WaylandApi: compositor chose client-side decorations; the window is undecorated");
  }

void WaylandApi::Private::onWpFractionalScaleV1PreferredScale(void* data, [[maybe_unused]] wp_fractional_scale_v1* fractionalScale, uint32_t scale) {
  auto w = static_cast<WWindow*>(data);
  w->preferredScale = scale;
  }

void WaylandApi::Private::onZwpLockedPointerV1Locked(void* data, [[maybe_unused]] zwp_locked_pointer_v1* lockedPointer) {
  static_cast<WWindow*>(data)->pointerLocked = true;
  }

void WaylandApi::Private::onZwpLockedPointerV1Unlocked(void* data, [[maybe_unused]] zwp_locked_pointer_v1* lockedPointer) {
  // E.g. focus lost; the persistent lock may become active again later.
  static_cast<WWindow*>(data)->pointerLocked = false;
  }

////
///     LISTENER TABLES
//  >>  Note: Every event of the bound interface version needs a handler: libwayland aborts on null entry.

///     Session listener tables
//
const wl_registry_listener WaylandApi::Private::wlRegistryListener = {
  .global          = onWlRegistryGlobal,
  .global_remove   = ioIgnore,
  };
const xdg_wm_base_listener WaylandApi::Private::xdgWmBaseListener = {
  .ping            = onXdgWmBasePing,
  };
const wl_seat_listener WaylandApi::Private::wlSeatListener = {
  .capabilities    = onWlSeatCapabilities,
  .name            = ioIgnore,
  };
const wl_pointer_listener WaylandApi::Private::wlPointerListener = {
  .enter           = onWlPointerEnter,
  .leave           = onWlPointerLeave,
  .motion          = onWlPointerMotion,
  .button          = onWlPointerButton,
  // TODO(wayland): touchpad scrolling: continuous axis events (axis_source finger) come without axis_value120
  .axis            = ioIgnore,
  .frame           = ioIgnore,
  .axis_source     = ioIgnore,
  .axis_stop       = ioIgnore,
  .axis_discrete   = ioIgnore, // not sent since v8 (replaced by axis_value120)
  .axis_value120   = onWlPointerAxisValue120,
  // axis_relative_direction (only v9+)
  };
const zwp_relative_pointer_v1_listener WaylandApi::Private::zwpRelativePointerV1Listener = {
  .relative_motion = onZwpRelativePointerV1RelativeMotion,
  };
const wl_keyboard_listener WaylandApi::Private::wlKeyboardListener = {
  .keymap          = onWlKeyboardKeymap,
  .enter           = onWlKeyboardEnter,
  .leave           = onWlKeyboardLeave,
  .key             = onWlKeyboardKey,
  .modifiers       = onWlKeyboardModifiers,
  .repeat_info     = onWlKeyboardRepeatInfo,
  };

///     Window specific listener tables
//
const wl_surface_listener WaylandApi::Private::wlSurfaceListener = {
  // Outputs and buffer scale hints; the scale comes from wp_fractional_scale_v1.
  .enter                      = ioIgnore,
  .leave                      = ioIgnore,
  .preferred_buffer_scale     = ioIgnore,
  .preferred_buffer_transform = ioIgnore,
  };
const xdg_surface_listener WaylandApi::Private::xdgSurfaceListener = {
  .configure                  = onXdgSurfaceConfigure,
  };
const xdg_toplevel_listener WaylandApi::Private::xdgToplevelListener = {
  .configure                  = onXdgToplevelConfigure,
  .close                      = onXdgToplevelClose,
  .configure_bounds           = ioIgnore,
  .wm_capabilities            = ioIgnore,
  };
const zxdg_toplevel_decoration_v1_listener WaylandApi::Private::zxdgToplevelDecorationV1Listener = {
  .configure                  = onZxdgToplevelDecorationV1Configure,
  };
const wp_fractional_scale_v1_listener WaylandApi::Private::wpFractionalScaleV1Listener = {
  .preferred_scale            = onWpFractionalScaleV1PreferredScale,
  };
const zwp_locked_pointer_v1_listener WaylandApi::Private::zwpLockedPointerV1Listener = {
  .locked                     = onZwpLockedPointerV1Locked,
  .unlocked                   = onZwpLockedPointerV1Unlocked,
  };

////
///     HELPERS
//

template<class T>
void WaylandApi::Private::ioDestroy(T*& obj, void (*destroyFn)(T*)) {
  // Destroys a Wayland/xkb object if it exists and clears the pointer: safe on half-built state and when called twice.
  if(obj==nullptr)
    return;
  destroyFn(obj);
  obj = nullptr;
  }

void WaylandApi::Private::logDisplayError() const {
  const int err = wl_display_get_error(display);
  if(err==EPROTO) {
    const wl_interface* iface = nullptr;
    uint32_t            id    = 0;
    const uint32_t      code  = wl_display_get_protocol_error(display, &iface, &id);
    Log::e("WaylandApi: protocol error ", code, " on ", (iface!=nullptr ? iface->name : "unknown"), "@", id);
    } else {
    Log::e("WaylandApi: connection error: ", std::strerror(err));
    }
  }

Event::MouseButton WaylandApi::Private::toButton(uint32_t button) {
  // wl_pointer.button carries evdev codes.
  switch(button) {
    case BTN_LEFT:   return Event::ButtonLeft;
    case BTN_RIGHT:  return Event::ButtonRight;
    case BTN_MIDDLE: return Event::ButtonMid;
    case BTN_SIDE:   return Event::ButtonBack;
    case BTN_EXTRA:  return Event::ButtonForward;
    }
  return Event::ButtonNone;
  }

wp_cursor_shape_device_v1_shape WaylandApi::Private::toCursorShape(CursorShape shape) {
  // Hidden is handled by the caller.
  switch(shape) {
    case CursorShape::Arrow:
    case CursorShape::Hidden:    return WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_DEFAULT;
    case CursorShape::IBeam:     return WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_TEXT;
    case CursorShape::SizeVer:   return WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_NS_RESIZE;
    case CursorShape::SizeHor:   return WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_EW_RESIZE;
    case CursorShape::SizeBDiag: return WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_NESW_RESIZE; // '/'
    case CursorShape::SizeFDiag: return WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_NWSE_RESIZE; // '\'
    case CursorShape::SizeAll:   return WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_ALL_SCROLL;  // four arrows; all_resize needs v2
    }
  return WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_DEFAULT;
  }

MouseEvent WaylandApi::Private::pointerEvent(const WWindow& w, Event::MouseButton button, int delta, Event::Type type) {
  // floor: the physical pixel the cursor is in.
  const int x = int(std::floor(w.appCursorX));
  const int y = int(std::floor(w.appCursorY));
  return MouseEvent(x, y, button, Event::M_NoModifier, delta, 0, type);
  }

AppEvent WaylandApi::Private::keyEvent(WWindow& w, uint32_t keycode, Event::Type type) const {
  // keycode: xkb keycode (evdev + 8). key: level-0 keysym of the active layout, so Shift+W stays K_W (like Windows'
  // virtual keys). code: the typed character, 0 if none (e.g. arrows, dead keys).
  // Built when recorded, not when dispatched: later 'modifiers' events change xkbState (Shift released, CapsLock).
  const xkb_layout_index_t layout = xkb_state_key_get_layout(xkbState, keycode);
  const xkb_keysym_t*      syms   = nullptr;
  const int                count  = xkb_keymap_key_get_syms_by_level(xkbKeymap, keycode, layout, 0, &syms);
  const xkb_keysym_t       sym    = count>0 ? syms[0] : XKB_KEY_NoSymbol; // choose first available
  const auto               key    = Event::KeyType(SystemApi::translateKey(sym));

  // Only characters up to U+FFFF (Basic Multilingual Plane): Tempest's text input can't hold more.
  // Beyond it, nothing is typed (X11 and Windows deliver half a UTF-16 surrogate pair there).
  const uint32_t utf32 = xkb_state_key_get_utf32(xkbState, keycode);
  const uint32_t code  = utf32<=0xFFFF ? utf32 : 0;
  return {.type=type, .w=&w, .key=key, .code=code, .keycode=keycode};
  }

bool WaylandApi::Private::isWindowFocused(const WWindow* w) const {
  // Window focus = keyboard focus: mouse moves, wheel and presses only reach the focused window.
  // Without a keyboard every window counts as focused (pointer events only come for the one under the pointer).
  return keyboard==nullptr || keyboardFocus==w;
  }

void WaylandApi::Private::setPointerLock(WWindow& w, bool lock) {
  // Mouse-look: Wayland apps can't move the pointer, so while the cursor is hidden it's locked in place and the app
  // gets relative motion instead (Tempest has no relative-mouse API). Called on every cursor change.
  // The lock stays requested: the compositor pauses it while the window is unfocused and resumes it by itself.
  if(lock && w.lockedPointer==nullptr && pointer!=nullptr) {
    w.lockedPointer = zwp_pointer_constraints_v1_lock_pointer(pointerConstraints, w.surface, pointer, nullptr,
                                                              ZWP_POINTER_CONSTRAINTS_V1_LIFETIME_PERSISTENT);
    zwp_locked_pointer_v1_add_listener(w.lockedPointer, &zwpLockedPointerV1Listener, &w);
    }
  else if(!lock && w.lockedPointer!=nullptr) {
    ioDestroy(w.lockedPointer, zwp_locked_pointer_v1_destroy);
    w.pointerLocked = false;
    }
  }

void WaylandApi::Private::releaseHeldKeys(WWindow& w) {
  repeatKeycode = 0;
  for(uint32_t keycode:heldKeys)
    appEvents.push_back(keyEvent(w, keycode, Event::KeyUp));
  heldKeys.clear();
  }

void WaylandApi::Private::releaseHeldButtons(WWindow& w) {
  for(uint32_t button:heldButtons)
    appEvents.push_back({.type=Event::MouseUp, .w=&w, .button=toButton(button)});
  heldButtons.clear();
  }

void WaylandApi::Private::releasePointer() {
  // release what the app saw pressed while pointer still exists
  if(pointerFocus!=nullptr)
    releaseHeldButtons(*pointerFocus);
  // Objects created from the pointer first.
  for(auto& w:windows)
    setPointerLock(*w, false);
  ioDestroy(relativePointer,   zwp_relative_pointer_v1_destroy);
  ioDestroy(cursorShapeDevice, wp_cursor_shape_device_v1_destroy);
  static_assert(wlSeatVersion>=WL_POINTER_RELEASE_SINCE_VERSION);
  ioDestroy(pointer,           wl_pointer_release);
  pointerFocus = nullptr;
  heldButtons.clear();
  }

void WaylandApi::Private::releaseKeyboard() {
  // release what the app saw pressed while keyboard still exists
  if(keyboardFocus!=nullptr)
    releaseHeldKeys(*keyboardFocus);
  static_assert(wlSeatVersion>=WL_KEYBOARD_RELEASE_SINCE_VERSION);
  ioDestroy(keyboard, wl_keyboard_release);
  keyboardFocus = nullptr;
  repeatKeycode = 0;
  heldKeys.clear();
  }

void WaylandApi::Private::startKeyRepeat(uint32_t keycode) {
  // Non-repeating keys (modifiers) leave a running repeat alone; a repeating key takes over.
  if(repeatRate<=0 || !xkb_keymap_key_repeats(xkbKeymap, keycode))
    return;
  repeatKeycode = keycode;
  repeatNext    = std::chrono::steady_clock::now() + std::chrono::milliseconds(repeatDelay);
  }

void WaylandApi::Private::processKeyRepeat() {
  if(repeatKeycode==0)
    return;
  const auto now      = std::chrono::steady_clock::now();
  const auto interval = std::chrono::microseconds(std::chrono::seconds(1))/repeatRate;
  if(now<repeatNext)
    return;
  repeatNext += interval;
  if(repeatNext<=now) // avoid bursts
    repeatNext = now + interval;
  appEvents.push_back(keyEvent(*keyboardFocus, repeatKeycode, Event::KeyDown));
  }

void WaylandApi::Private::dispatchCloseEvent(WWindow& w) {
  // Only a request. Like the Windows backend: the app or an overlay may accept the CloseEvent; otherwise exit.
  // w isn't used after the dispatch: the app may destroy the window while handling it.
  if(w.owner==nullptr)
    return;
  CloseEvent e;
  SystemApi::dispatchClose(*w.owner, e);
  if(!e.isAccepted())
    SystemApi::exit();
  }

void WaylandApi::Private::dispatchKeyEvent(const AppEvent& e) {
  if(e.w->owner==nullptr)
    return;
  KeyEvent k(e.key, e.code, Event::M_NoModifier, e.type);
  if(e.type==Event::KeyDown)
    SystemApi::dispatchKeyDown(*e.w->owner, k, e.keycode);
  else
    SystemApi::dispatchKeyUp(*e.w->owner, k, e.keycode);
  }

void WaylandApi::Private::dispatchFocusEvent(const AppEvent& e) {
  if(e.w->owner==nullptr)
    return;
  FocusEvent f(e.focusIn, Event::UnknownReason);
  SystemApi::dispatchFocus(*e.w->owner, f);
  }

void WaylandApi::Private::dispatchMouseEvent(const AppEvent& e) {
  // A move updates the app's cursor (see WWindow::appCursorX)
  if(e.w->owner==nullptr)
    return;
  if(e.type==Event::MouseMove) {
    e.w->appCursorX = e.x;
    e.w->appCursorY = e.y;
    }
  MouseEvent m = pointerEvent(*e.w, e.button, e.delta, e.type);
  switch(e.type) {
    case Event::MouseMove:
      SystemApi::dispatchMouseMove(*e.w->owner, m);
      break;
    case Event::MouseDown:
      SystemApi::dispatchMouseDown(*e.w->owner, m);
      break;
    case Event::MouseUp:
      SystemApi::dispatchMouseUp(*e.w->owner, m);
      break;
    case Event::MouseWheel:
      SystemApi::dispatchMouseWheel(*e.w->owner, m);
      break;
    default:
      break;
    }
  }

////
///     CONNECTION AND WINDOWS
//

bool WaylandApi::Private::hasRequiredGlobals() const {
  // No fallbacks yet without fractional-scale, cursor-shape or server-side decorations (e.g. GNOME doesn't work).
  // Pointer constraints + relative pointer: mouse-look, since Wayland clients can't warp the pointer.
  struct Required {
    const void*         object;
    const wl_interface& interface;
    uint32_t            version;
    };
  const Required required[] = {
    {compositor,             wl_compositor_interface,                   wlCompositorVersion               },
    {wmBase,                 xdg_wm_base_interface,                     xdgWmBaseVersion                  },
    {decorationManager,      zxdg_decoration_manager_v1_interface,      zxdgDecorationManagerV1Version    },
    {viewporter,             wp_viewporter_interface,                   wpViewporterVersion               },
    {fractionalScaleManager, wp_fractional_scale_manager_v1_interface,  wpFractionalScaleManagerV1Version },
    {seat,                   wl_seat_interface,                         wlSeatVersion                     },
    {cursorShapeManager,     wp_cursor_shape_manager_v1_interface,      wpCursorShapeManagerV1Version     },
    {pointerConstraints,     zwp_pointer_constraints_v1_interface,      zwpPointerConstraintsV1Version    },
    {relativePointerManager, zwp_relative_pointer_manager_v1_interface, zwpRelativePointerManagerV1Version},
    };
  bool hasAll = true;
  for(auto& r:required) {
    if(r.object!=nullptr)
      continue;
    Log::e("WaylandApi: compositor doesn't offer ", r.interface.name, " version ", r.version, " or newer");
    hasAll = false;
    }
  return hasAll;
  }

bool WaylandApi::Private::connect() {
  // True only if Wayland is usable (connected + all required globals). Otherwise logs error
  // Note: ::dpy set in constructor/destructor only!

  // TODO(wayland): WAYLAND_DISPLAY check: wl_display_connect(nullptr) falls back to "wayland-0",
  // which may belong to another session of the same user (e.g. X11 session here, Wayland session
  // on another Virtual Terminal).

  display = wl_display_connect(nullptr);
  if(!display) {
    const int   err  = errno;
    const char* name = std::getenv("WAYLAND_DISPLAY");
    Log::e("WaylandApi: can't connect to a compositor (WAYLAND_DISPLAY=", (name!=nullptr ? name : "not set"), "): ",
           std::strerror(err));
    return false;
    }

  // Needed to compile keymap: without it there's no keyboard input
  xkbContext = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
  if(!xkbContext) {
    Log::e("WaylandApi: xkb_context_new failed");
    return false;
    }

  registry = wl_display_get_registry(display);
  wl_registry_add_listener(registry, &wlRegistryListener, this);

  if(wl_display_roundtrip(display)<0) {
    logDisplayError();
    return false;
    }
  return hasRequiredGlobals();
  }

WaylandApi::Private::~Private() {
  // Also runs for a half-connected state when the WaylandApi constructor throws.
  // Connection errors are ignored while tearing down.
  if(!display)
    return;

  // Windows first: xdg_wm_base must not be destroyed while xdg_surfaces exist
  while(!windows.empty())
    destroyWindow(windows.back().get());

  ioDestroy(relativePointerManager, zwp_relative_pointer_manager_v1_destroy);
  ioDestroy(pointerConstraints,     zwp_pointer_constraints_v1_destroy);
  ioDestroy(cursorShapeManager,     wp_cursor_shape_manager_v1_destroy);
  ioDestroy(fractionalScaleManager, wp_fractional_scale_manager_v1_destroy);
  ioDestroy(viewporter,             wp_viewporter_destroy);
  ioDestroy(decorationManager,      zxdg_decoration_manager_v1_destroy);
  ioDestroy(wmBase,                 xdg_wm_base_destroy);

  // Seat devices before the seat
  releaseKeyboard();
  releasePointer();

  ioDestroy(xkbState,   xkb_state_unref);
  ioDestroy(xkbKeymap,  xkb_keymap_unref);
  ioDestroy(xkbContext, xkb_context_unref);

  static_assert(wlSeatVersion>=WL_SEAT_RELEASE_SINCE_VERSION);
  ioDestroy(seat,       wl_seat_release);
  ioDestroy(compositor, wl_compositor_destroy);
  ioDestroy(registry,   wl_registry_destroy);

  wl_display_flush(display);
  wl_display_disconnect(display);
  }

SystemApi::Window* WaylandApi::Private::createWindow(Tempest::Window* owner, uint32_t width, uint32_t height, SystemApi::ShowMode sm) {
  // all roles and state requests needed before the first commit
  windows.emplace_back(std::make_unique<WWindow>());
  WWindow* win = windows.back().get();
  win->owner = owner;

  win->surface = wl_compositor_create_surface(compositor);
  wl_surface_add_listener(win->surface, &wlSurfaceListener, win); // also attaches WWindow user data to wl_surface

  win->xdgSurface = xdg_wm_base_get_xdg_surface(wmBase, win->surface);
  xdg_surface_add_listener(win->xdgSurface, &xdgSurfaceListener, win);
  win->toplevel = xdg_surface_get_toplevel(win->xdgSurface);
  xdg_toplevel_add_listener(win->toplevel, &xdgToplevelListener, win);

  win->decoration = zxdg_decoration_manager_v1_get_toplevel_decoration(decorationManager, win->toplevel);
  zxdg_toplevel_decoration_v1_set_mode(win->decoration, ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE);
  zxdg_toplevel_decoration_v1_add_listener(win->decoration, &zxdgToplevelDecorationV1Listener, win);

  win->viewport = wp_viewporter_get_viewport(viewporter, win->surface);

  win->fractionalScale = wp_fractional_scale_manager_v1_get_fractional_scale(fractionalScaleManager, win->surface);
  wp_fractional_scale_v1_add_listener(win->fractionalScale, &wpFractionalScaleV1Listener, win);

  // Requested before the first commit, so the first configure already carries the maximized/fullscreen size.
  if(sm==SystemApi::Maximized)
    xdg_toplevel_set_maximized(win->toplevel);
  if(sm==SystemApi::FullScreen)
    xdg_toplevel_set_fullscreen(win->toplevel, nullptr);
  // Only a request (xdg-shell doesn't say whether it works before mapping; KWin honors it).
  if(sm==SystemApi::Minimized)
    xdg_toplevel_set_minimized(win->toplevel);

  wl_surface_commit(win->surface);

  // Wait for the first configure: Window's constructor reads the client rect right after this returns,
  // and no buffer (= Vulkan present) may be attached before the first ack_configure.
  while(!win->configured) {
    if(wl_display_dispatch(display)<0) {
      logDisplayError();
      destroyWindow(win);
      return nullptr;
      }
    }

  if(win->width==0 || win->height==0) {
    // The compositor left the size to us: the app's size, physical -> logical
    // (the scale is known now, KWin sends preferred_scale before the first configure).
    win->width  = std::max(1, int32_t(std::lround(float(width) /win->scale())));
    win->height = std::max(1, int32_t(std::lround(float(height)/win->scale())));
    // Also in pending: later configures without a size keep it.
    win->pending.width  = win->width;
    win->pending.height = win->height;
    // Before the first buffer (Vulkan's first present), so it applies with that commit.
    wp_viewport_set_destination(win->viewport, win->width, win->height);
    }

  return reinterpret_cast<SystemApi::Window*>(win->surface); // wl_surface* native handle
  }

void WaylandApi::Private::destroyWindow(WWindow* w) {
  // Reverse order of creation; also used for a half-created window.
  // The VkSurfaceKHR must be gone before the wl_surface: Tempest destroys the swapchain before the window.
  if(w==nullptr)
    return;

  // The compositor sends wl_pointer/wl_keyboard.leave for a destroyed surface, but only after we've deleted w.
  if(pointerFocus==w) {
    pointerFocus = nullptr;
    heldButtons.clear();
    }
  if(keyboardFocus==w) {
    keyboardFocus = nullptr;
    repeatKeycode = 0;
    heldKeys.clear();
    heldButtons.clear();
    }
  if(focusGainedInBatch==w)
    focusGainedInBatch = nullptr;

  setPointerLock(*w, false); // created for the wl_surface
  ioDestroy(w->fractionalScale, wp_fractional_scale_v1_destroy);
  ioDestroy(w->viewport,        wp_viewport_destroy);
  ioDestroy(w->decoration,      zxdg_toplevel_decoration_v1_destroy); // before toplevel
  ioDestroy(w->toplevel,        xdg_toplevel_destroy);
  ioDestroy(w->xdgSurface,      xdg_surface_destroy);
  ioDestroy(w->surface,         wl_surface_destroy);

  // Our recorded events for w are dropped here; events still queued for its objects are dropped by libwayland
  std::erase_if(appEvents, [w](const AppEvent& e) { return e.w==w; });
  std::erase_if(windows, [w](const std::unique_ptr<WWindow>& p) { return p.get()==w; });
  }

////
///     EVENT LOOP
//

void WaylandApi::Private::handleConnectionError() {
  // connection dead, nothing works anymore, end the app
  logDisplayError();
  SystemApi::exit();
  }

int WaylandApi::Private::pollTimeout(const SystemApi::AppCallBack& cb) const {
  // 0 if a window may render; otherwise wait for the compositor (e.g. a configure that ends 'suspended'),
  // but not beyond the next timer or key repeat, and at most idlePollTimeoutMs

  // Longest sleep in poll() while no window may render
  constexpr int idlePollTimeoutMs = 5;
  const bool canRender = std::any_of(windows.begin(), windows.end(),
                                     [](const std::unique_ptr<WWindow>& w) { return w->readyToRender(); });
  if(canRender)
    return 0;
  int timeout = int(std::min<uint64_t>(cb.timeToTimer(), idlePollTimeoutMs));
  if(repeatKeycode!=0) {
    const auto untilRepeat = std::chrono::ceil<std::chrono::milliseconds>(repeatNext - std::chrono::steady_clock::now());
    timeout = std::clamp(int(untilRepeat.count()), 0, timeout);
    }
  return timeout;
  }

void WaylandApi::Private::readEvents(const SystemApi::AppCallBack& cb) {
  // Doesn't block beyond pollTimeout(): Tempest's loop must keep running (timers, rendering).
  // The Vulkan driver reads the same connection on its own queue; prepare_read -> read_events/cancel_read
  // is how several readers share the socket without stealing or blocking each other.
  focusGainedInBatch = nullptr; // a new batch (see onWlPointerButton)
  while(wl_display_prepare_read(display)!=0) {
    // Our queue already holds events (e.g. read by the driver): dispatch them before reading more.
    if(wl_display_dispatch_pending(display)<0) {
      handleConnectionError();
      return;
      }
    }

  // Send our requests even when no frame is presented. If the socket is full (EAGAIN), the rest goes out next time.
  wl_display_flush(display);

  pollfd fd = {};
  fd.fd     = wl_display_get_fd(display);
  fd.events = POLLIN;
  // Timeout computed after dispatching the queued events: a configure among them may let a window render now.
  if(poll(&fd, 1, pollTimeout(cb))>0) {
    if(wl_display_read_events(display)<0) {
      handleConnectionError();
      return;
      }
    } else {
    wl_display_cancel_read(display);
    }

  if(wl_display_dispatch_pending(display)<0) {
    handleConnectionError();
    return;
    }

  processKeyRepeat(); // synthetic key events
  }

bool WaylandApi::Private::dispatchAppEvent() {
  // At most one event. Taken out before dispatching, so a nested event loop continues with the next one.
  if(appEvents.empty())
    return false;
  const AppEvent e = appEvents.front();
  appEvents.pop_front();
  switch(e.type) {
    case Event::Close:
      dispatchCloseEvent(*e.w);
      break;
    case Event::KeyDown:
    case Event::KeyUp:
      dispatchKeyEvent(e);
      break;
    case Event::Focus:
      dispatchFocusEvent(e);
      break;
    case Event::MouseMove:
    case Event::MouseDown:
    case Event::MouseUp:
    case Event::MouseWheel:
      dispatchMouseEvent(e);
      break;
    default:
      break;
    }
  return true;
  }

void WaylandApi::Private::updateState(WWindow& w) {
  // Update things that should change only once per draw, not on every triggered callback (e.g. swapchain resize)
  const int32_t pw = w.physicalWidth();
  const int32_t ph = w.physicalHeight();
  // The app's size: set from the client rect by Window's constructor, then by each SizeEvent
  if(pw!=w.owner->w() || ph!=w.owner->h()) {
    SizeEvent e(pw, ph);
    SystemApi::dispatchResize(*w.owner, e);
    }
  }

void WaylandApi::Private::renderWindows() {
  // Skips suspended windows: nothing is drawn while the compositor doesn't repaint them (e.g. minimized).
  for(auto& w:windows) {
    if(!w->readyToRender())
      continue;
    updateState(*w);
    SystemApi::dispatchRender(*w->owner);
    }
  }

////
///     WAYLAND API
//

WaylandApi::WaylandApi() {
  // Same table as X11: XKB keysyms have the values of X11's keysyms. Letters, digits and F-keys are ranges
  // (see SystemApi::translateKey).
  static const TranslateKeyPair k[] = {
    { XKB_KEY_Control_L, Event::K_LControl },
    { XKB_KEY_Control_R, Event::K_RControl },

    { XKB_KEY_Shift_L,   Event::K_LShift   },
    { XKB_KEY_Shift_R,   Event::K_RShift   },

    { XKB_KEY_Alt_L,     Event::K_LAlt     },
    { XKB_KEY_Alt_R,     Event::K_RAlt     },

    { XKB_KEY_Insert,    Event::K_Insert   },
    { XKB_KEY_Delete,    Event::K_Delete   },
    { XKB_KEY_Home,      Event::K_Home     },
    { XKB_KEY_End,       Event::K_End      },
    { XKB_KEY_Page_Up,   Event::K_PageUp   },
    { XKB_KEY_Page_Down, Event::K_PageDown },

    { XKB_KEY_Left,      Event::K_Left     },
    { XKB_KEY_Right,     Event::K_Right    },
    { XKB_KEY_Up,        Event::K_Up       },
    { XKB_KEY_Down,      Event::K_Down     },

    { XKB_KEY_Escape,    Event::K_ESCAPE   },
    { XKB_KEY_Tab,       Event::K_Tab      },
    { XKB_KEY_BackSpace, Event::K_Back     },
    { XKB_KEY_Pause,     Event::K_Pause    },
    { XKB_KEY_Return,    Event::K_Return   },
    { XKB_KEY_space,     Event::K_Space    },
    { XKB_KEY_Caps_Lock, Event::K_CapsLock },

    { XKB_KEY_F1,        Event::K_F1       },
    { XKB_KEY_0,         Event::K_0        },
    { XKB_KEY_a,         Event::K_A        },

    { 0,                 Event::K_NoKey    }
    };
  setupKeyTranslate(k,24);

  impl.reset(new Private());
  if(!impl->connect()) {
    // impl's destructor cleans up the half-connected state
    Log::e("WaylandApi: Wayland is not usable (use TEMPEST_DISABLE_WAYLAND=1 for X11)");
    throw std::system_error(Tempest::SystemErrc::UnableToConnectDisplay);
    }
  dpy = impl->display;
  }

WaylandApi::~WaylandApi() {
  dpy = nullptr;
  }

bool WaylandApi::isDisabled() {
  // Manual override, e.g. to compare with the X11 backend: TEMPEST_DISABLE_WAYLAND=1
  const char* disable = std::getenv("TEMPEST_DISABLE_WAYLAND");
  return disable!=nullptr && std::strcmp(disable,"1")==0;
  }

wl_display* WaylandApi::display() {
  SystemApi::inst();
  return dpy;
  }

SystemApi::Window* WaylandApi::implCreateWindow(Tempest::Window* owner, uint32_t width, uint32_t height) {
  return impl->createWindow(owner,width,height,SystemApi::Normal);
  }

SystemApi::Window* WaylandApi::implCreateWindow(Tempest::Window* owner, ShowMode sm) {
  // 800x600 like the other backends; Maximized/FullScreen get their size from the first configure.
  // Hidden treated as Normal.
  return impl->createWindow(owner,800,600,sm);
  }

void WaylandApi::implDestroyWindow(SystemApi::Window* w) {
  impl->destroyWindow(toWWindow(w));
  }

void WaylandApi::implExit() {
  isExit.store(true);
  }

Rect WaylandApi::implWindowClientRect(SystemApi::Window* w) {
  // Wayland has no global window position.
  auto ww = toWWindow(w);
  return Rect(0, 0, ww->physicalWidth(), ww->physicalHeight());
  }

bool WaylandApi::implSetAsFullscreen(SystemApi::Window* w, bool fullScreen) {
  // Only a request: implIsFullscreen() and the size follow with the next configure.
  auto ww = toWWindow(w);
  if(fullScreen)
    xdg_toplevel_set_fullscreen(ww->toplevel, nullptr); // nullptr: the compositor picks the output
  else
    xdg_toplevel_unset_fullscreen(ww->toplevel);
  wl_display_flush(impl->display);
  return true;
  }

bool WaylandApi::implIsFullscreen(SystemApi::Window* w) {
  return toWWindow(w)->fullscreen;
  }

float WaylandApi::implUiScale(SystemApi::Window* w) {
  return toWWindow(w)->scale();
  }

void WaylandApi::implSetWindowTitle(SystemApi::Window* w, const char* utf8) {
  xdg_toplevel_set_title(toWWindow(w)->toplevel, utf8);
  }

void WaylandApi::implSetCursorPosition(SystemApi::Window* w, int x, int y) {
  // Wayland clients can't move the pointer. While it's locked, the virtual cursor is all the app sees, so setting it is
  // enough (no MouseMove for the jump).
  auto ww = toWWindow(w);
  if(!ww->pointerLocked)
    return;
  ww->appCursorX = x;
  ww->appCursorY = y;
  }

void WaylandApi::implShowCursor(SystemApi::Window* w, CursorShape show) {
  // The pointer lock is requested whenever the cursor is hidden, even with the pointer elsewhere: the compositor
  // activates it once the pointer is over the focused window (see Private::setPointerLock).
  // The cursor image needs the enter serial, so it's only set while the pointer is over w; enter and focus changes
  // re-apply it. An unfocused window shows the desktop cursor.
  auto ww = toWWindow(w);
  impl->setPointerLock(*ww, show==CursorShape::Hidden);
  if(impl->pointerFocus!=ww)
    return;
  const CursorShape shape = impl->isWindowFocused(ww) ? show : CursorShape::Arrow;
  if(shape==CursorShape::Hidden)
    wl_pointer_set_cursor(impl->pointer, impl->pointerEnterSerial, nullptr, 0, 0);
  else
    wp_cursor_shape_device_v1_set_shape(impl->cursorShapeDevice, impl->pointerEnterSerial, Private::toCursorShape(shape));
  }

bool WaylandApi::implIsRunning() {
  return !isExit.load();
  }

int WaylandApi::implExec(SystemApi::AppCallBack& cb) {
  while(!isExit.load()) {
    implProcessEvents(cb);
    }
  return 0;
  }

void WaylandApi::implProcessEvents(SystemApi::AppCallBack& cb) {
  // Like X11: one app event per call, else timers and rendering. Wayland events are read (all at once, libwayland
  // can't do less) only when no app event is left; their listeners only record. Configures change state only, so they
  // can't starve rendering. readEvents() only waits while no window may render (see pollTimeout()).
  if(impl->dispatchAppEvent())
    return;
  impl->readEvents(cb);
  if(impl->dispatchAppEvent())
    return;
  if(cb.onTimer()==0)
    std::this_thread::yield();
  impl->renderWindows();
  }

#endif // defined(TEMPEST_BUILD_WAYLAND)
