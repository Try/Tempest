#include "waylandapi.h"

#if defined(TEMPEST_BUILD_WAYLAND)
#include <Tempest/Event>
#include <Tempest/Window>
#include <Tempest/Log>

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
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

// useful to silence Wayland listener stubs
#define IO_WL_STUB_QUIET(Name) .Name = [](auto...) {}

using namespace Tempest;

static wl_display*      dpy = nullptr; // non-null while Wayland is the active backend; written only by ctor/dtor of WaylandApi
static std::atomic_bool isExit{0};
static std::atomic_bool requested{0};

// Per-window state. The SystemApi::Window* handed to Tempest points to this struct.
struct WWindow {
  Tempest::Window*             owner           = nullptr;

  wl_surface*                  surface         = nullptr;
  xdg_surface*                 xdgSurface      = nullptr;
  xdg_toplevel*                toplevel        = nullptr;
  zxdg_toplevel_decoration_v1* decoration      = nullptr;
  wp_viewport*                 viewport        = nullptr;
  wp_fractional_scale_v1*      fractionalScale = nullptr;

  // Announced by xdg_toplevel.configure; becomes current on xdg_surface.configure.
  struct Pending {
    int32_t width      = 0; // logical; 0 = no size from the compositor yet
    int32_t height     = 0;
    bool    fullscreen = false;
    } pending;

  // Current state
  int32_t  width          = 0;     // logical
  int32_t  height         = 0;
  uint32_t preferredScale = 120;   // wp_fractional_scale_v1: scale * 120
  bool     fullscreen     = false;
  bool     configured     = false; // first configure acked; no buffer (= Vulkan present) before that

  int32_t  appRequestedWidth  = 0; // physical; used until the compositor sends a size
  int32_t  appRequestedHeight = 0;

  // Physical size the app last saw; updateState() dispatches a resize when it differs.
  int32_t  dispatchedWidth  = 0;
  int32_t  dispatchedHeight = 0;

  // One frame callback per present, kept until the compositor has shown it: their number is how many presents it
  // hasn't shown yet. Rendering pauses at maxFramesAhead (e.g. while minimized). maxFramesAhead comes from the swapchain.
  std::vector<wl_callback*> frameCallbacks;
  uint32_t                  maxFramesAhead = 0;

  // Pointer lock for mouse-look, requested while the window's cursor is hidden (see Private::setPointerLock).
  zwp_locked_pointer_v1* lockedPointer = nullptr;
  bool                   pointerLocked = false;

  // Cursor position in physical pixels (button and axis events carry none). While the pointer is locked, this is a
  // virtual cursor moved by relative motion.
  double                 cursorX       = 0;
  double                 cursorY       = 0;

  bool    readyToRender() const  { return owner!=nullptr && configured && (frameCallbacks.empty() || frameCallbacks.size()<maxFramesAhead); }

  float   scale() const          { return float(preferredScale)/120.f; }
  // Physical size, as Tempest sees it (client rect, resize events, swapchain extent). Rounded half up as fractional-scale
  // requires; in integers, since scales like 1.15 aren't exact as float.
  int32_t physicalWidth() const  { return int32_t((int64_t(width) *preferredScale + 60)/120); }
  int32_t physicalHeight() const { return int32_t((int64_t(height)*preferredScale + 60)/120); }
  };

static WWindow* toWWindow(SystemApi::Window* w) {
  return reinterpret_cast<WWindow*>(w);
  }

static Event::MouseButton toButton(uint32_t button) {
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

static wp_cursor_shape_device_v1_shape toCursorShape(CursorShape shape) {
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

struct WaylandApi::Private {
  wl_display*                     display                = nullptr;
  wl_registry*                    registry               = nullptr;

  // Globals
  wl_compositor*                  compositor             = nullptr;
  wl_seat*                        seat                   = nullptr;
  xdg_wm_base*                    wmBase                 = nullptr;
  zxdg_decoration_manager_v1*     decorationManager      = nullptr;
  wp_viewporter*                  viewporter             = nullptr;
  wp_fractional_scale_manager_v1* fractionalScaleManager = nullptr;
  wp_cursor_shape_manager_v1*     cursorShapeManager     = nullptr;
  zwp_pointer_constraints_v1*     pointerConstraints     = nullptr;
  zwp_relative_pointer_manager_v1* relativePointerManager = nullptr;

  // Exact versions we need to bind successfully: the listeners and requests are written for these.
  static constexpr uint32_t wlCompositorVersion               = 6;
  static constexpr uint32_t wlSeatVersion                     = 8; // v8: wl_pointer.axis_value120 (wheel in 1/120 notches)
  static constexpr uint32_t xdgWmBaseVersion                  = 1;
  static constexpr uint32_t zxdgDecorationManagerV1Version    = 1;
  static constexpr uint32_t wpViewporterVersion               = 1;
  static constexpr uint32_t wpFractionalScaleManagerV1Version = 1;
  static constexpr uint32_t wpCursorShapeManagerV1Version     = 1;
  static constexpr uint32_t zwpPointerConstraintsV1Version    = 1;
  static constexpr uint32_t zwpRelativePointerManagerV1Version = 1;

  // Longest sleep in poll() while no window may render; short enough to keep Tempest's timers accurate.
  static constexpr int      idlePollTimeoutMs                 = 5;

  // Input devices, non-null while the seat has the capability (can change at runtime)
  wl_pointer*                     pointer                = nullptr;
  wp_cursor_shape_device_v1*      cursorShapeDevice      = nullptr; // exists together with 'pointer'
  zwp_relative_pointer_v1*        relativePointer        = nullptr; // exists together with 'pointer'
  wl_keyboard*                    keyboard               = nullptr;

  // Pointer state between wl_pointer.enter and leave; the position is in WWindow::cursorX/Y.
  WWindow*                        pointerFocus           = nullptr; // window under the pointer
  uint32_t                        pointerEnterSerial     = 0;       // set_cursor/set_shape need the serial of the latest enter

  // Keymap from wl_keyboard.keymap, state follows wl_keyboard.modifiers; both null until the first keymap.
  xkb_context*                    xkbContext             = nullptr;
  xkb_keymap*                     xkbKeymap              = nullptr;
  xkb_state*                      xkbState               = nullptr;
  WWindow*                        keyboardFocus          = nullptr; // window with keyboard focus (wl_keyboard.enter/leave)

  // Keys (xkb keycodes) and buttons (evdev codes) whose press reached the app. Only their releases are dispatched,
  // and they're released on focus or device loss (see releaseHeldInput).
  std::vector<uint32_t>           heldKeys;
  std::vector<uint32_t>           heldButtons;

  // Key repeat: Wayland sends none. Manual implementation from wl_keyboard.repeat_info by sending the held key's KeyDown again,
  // which Tempest's dispatcher turns into KeyRepeat (like on Windows).
  int32_t                         repeatRate             = 0;       // repeats per second; 0 = disabled
  int32_t                         repeatDelay            = 0;       // ms until the first repeat trigger
  uint32_t                        repeatKeycode          = 0;       // xkb keycode of the repeating key; 0 = none.
                                                                     // Non-zero only while keyboardFocus is set and repeatRate>0.
  std::chrono::steady_clock::time_point repeatNext;                 // when the next repeat is due

  // WWindow* if a window gained focus during active readEvents() batch, otherwise nullptr (focus click filter)
  WWindow*                        focusGainedInBatch     = nullptr;

  std::vector<std::unique_ptr<WWindow>> windows; // owns the windows; each WWindow keeps its address (handle, listener data)

  bool     connect();
  void     disconnect();
  void     releasePointer();
  void     setPointerLock(WWindow& w, bool lock);
  void     releaseKeyboard();
  bool     hasRequiredGlobals() const;
  void     logDisplayError() const;

  WWindow* createWindow(Tempest::Window* owner, uint32_t width, uint32_t height, SystemApi::ShowMode sm);
  void     destroyWindow(WWindow* w);
  static void applyConfigure(WWindow& w, uint32_t serial);
  static void updateState(WWindow& w);

  bool       isWindowFocused(const WWindow* w) const;
  void       releaseHeldInput(WWindow& w);
  void       releaseHeldKeys(WWindow& w);
  void       releaseHeldButtons(WWindow& w);
  static MouseEvent pointerEvent(const WWindow& w, Event::MouseButton button, int delta, Event::Type type);
  KeyEvent   keyEvent(uint32_t keycode, Event::Type type) const;
  void       startKeyRepeat(uint32_t keycode);
  void       processKeyRepeat();

  int      pollTimeout() const;
  void     readEvents();
  void     handleConnectionError();
  bool     hasWindowToRender() const;
  void     renderWindows();

  // Session listeners (globals and seat devices). 'data' is Private*.
  static void onWlRegistryGlobal(void* data, wl_registry* registry, uint32_t name, const char* interface, uint32_t version);
  static void onXdgWmBasePing(void* data, xdg_wm_base* wmBase, uint32_t serial);
  static void onWlSeatCapabilities(void* data, wl_seat* seat, uint32_t capabilities);
  static void onWlPointerEnter(void* data, wl_pointer* pointer, uint32_t serial, wl_surface* surface, wl_fixed_t x, wl_fixed_t y);
  static void onWlPointerLeave(void* data, wl_pointer* pointer, uint32_t serial, wl_surface* surface);
  static void onWlPointerMotion(void* data, wl_pointer* pointer, uint32_t time, wl_fixed_t x, wl_fixed_t y);
  static void onWlPointerButton(void* data, wl_pointer* pointer, uint32_t serial, uint32_t time, uint32_t button, uint32_t state);
  static void onZwpRelativePointerV1RelativeMotion(void* data, zwp_relative_pointer_v1* relativePointer, uint32_t utimeHi,
                                                   uint32_t utimeLo, wl_fixed_t dx, wl_fixed_t dy,
                                                   wl_fixed_t dxUnaccel, wl_fixed_t dyUnaccel);
  static void onWlPointerAxisValue120(void* data, wl_pointer* pointer, uint32_t axis, int32_t value120);
  static void onWlKeyboardKeymap(void* data, wl_keyboard* keyboard, uint32_t format, int32_t fd, uint32_t size);
  static void onWlKeyboardEnter(void* data, wl_keyboard* keyboard, uint32_t serial, wl_surface* surface, wl_array* keys);
  static void onWlKeyboardLeave(void* data, wl_keyboard* keyboard, uint32_t serial, wl_surface* surface);
  static void onWlKeyboardKey(void* data, wl_keyboard* keyboard, uint32_t serial, uint32_t time, uint32_t key, uint32_t state);
  static void onWlKeyboardModifiers(void* data, wl_keyboard* keyboard, uint32_t serial, uint32_t depressed,
                                    uint32_t latched, uint32_t locked, uint32_t group);
  static void onWlKeyboardRepeatInfo(void* data, wl_keyboard* keyboard, int32_t rate, int32_t delay);

  static const wl_registry_listener                 wlRegistryListener;
  static const xdg_wm_base_listener                 xdgWmBaseListener;
  static const wl_seat_listener                     wlSeatListener;
  static const wl_pointer_listener                  wlPointerListener;
  static const zwp_relative_pointer_v1_listener     zwpRelativePointerV1Listener;
  static const wl_keyboard_listener                 wlKeyboardListener;

  // Window specific listeners. 'data' is WWindow*.
  static void onXdgSurfaceConfigure(void* data, xdg_surface* xdgSurface, uint32_t serial);
  static void onXdgToplevelConfigure(void* data, xdg_toplevel* toplevel, int32_t width, int32_t height, wl_array* states);
  static void onXdgToplevelClose    (void* data, xdg_toplevel* toplevel);
  static void onZxdgToplevelDecorationV1Configure(void* data, zxdg_toplevel_decoration_v1* decoration, uint32_t mode);
  static void onWpFractionalScaleV1PreferredScale(void* data, wp_fractional_scale_v1* fractionalScale, uint32_t scale);
  static void onWlCallbackDone(void* data, wl_callback* callback, uint32_t time);
  static void onZwpLockedPointerV1Locked  (void* data, zwp_locked_pointer_v1* lockedPointer);
  static void onZwpLockedPointerV1Unlocked(void* data, zwp_locked_pointer_v1* lockedPointer);

  static const wl_surface_listener                  wlSurfaceListener;
  static const xdg_surface_listener                 xdgSurfaceListener;
  static const xdg_toplevel_listener                xdgToplevelListener;
  static const zxdg_toplevel_decoration_v1_listener zxdgToplevelDecorationV1Listener;
  static const wp_fractional_scale_v1_listener      wpFractionalScaleV1Listener;
  static const wl_callback_listener                 wlCallbackListener;
  static const zwp_locked_pointer_v1_listener       zwpLockedPointerV1Listener;
  };

// Note: Every event of the bound interface version needs a handler: libwayland aborts on null entry.

// Session listeners

const wl_registry_listener WaylandApi::Private::wlRegistryListener = {
  .global        = onWlRegistryGlobal,
  IO_WL_STUB_QUIET(global_remove),
  };

const xdg_wm_base_listener WaylandApi::Private::xdgWmBaseListener = {
  .ping = onXdgWmBasePing,
  };

const wl_seat_listener WaylandApi::Private::wlSeatListener = {
  .capabilities = onWlSeatCapabilities,
  IO_WL_STUB_QUIET(name),
  };

const wl_pointer_listener WaylandApi::Private::wlPointerListener = {
  .enter         = onWlPointerEnter,
  .leave         = onWlPointerLeave,
  .motion        = onWlPointerMotion,
  .button        = onWlPointerButton,
  // TODO(wayland): touchpad scrolling: continuous axis events (axis_source finger) come without axis_value120
  IO_WL_STUB_QUIET(axis),
  IO_WL_STUB_QUIET(frame),
  IO_WL_STUB_QUIET(axis_source),
  IO_WL_STUB_QUIET(axis_stop),
  IO_WL_STUB_QUIET(axis_discrete), // not sent since v8 (replaced by axis_value120)
  .axis_value120 = onWlPointerAxisValue120,
  // axis_relative_direction (only v9+)
  };

const zwp_relative_pointer_v1_listener WaylandApi::Private::zwpRelativePointerV1Listener = {
  .relative_motion = onZwpRelativePointerV1RelativeMotion,
  };

const wl_keyboard_listener WaylandApi::Private::wlKeyboardListener = {
  .keymap      = onWlKeyboardKeymap,
  .enter       = onWlKeyboardEnter,
  .leave       = onWlKeyboardLeave,
  .key         = onWlKeyboardKey,
  .modifiers   = onWlKeyboardModifiers,
  .repeat_info = onWlKeyboardRepeatInfo,
  };

// Window specific listeners

const wl_surface_listener WaylandApi::Private::wlSurfaceListener = {
  // Outputs and buffer scale hints; the scale comes from wp_fractional_scale_v1.
  IO_WL_STUB_QUIET(enter),
  IO_WL_STUB_QUIET(leave),
  IO_WL_STUB_QUIET(preferred_buffer_scale),
  IO_WL_STUB_QUIET(preferred_buffer_transform),
  };

const xdg_surface_listener WaylandApi::Private::xdgSurfaceListener = {
  .configure = onXdgSurfaceConfigure,
  };

const xdg_toplevel_listener WaylandApi::Private::xdgToplevelListener = {
  .configure        = onXdgToplevelConfigure,
  .close            = onXdgToplevelClose,
  };

const zxdg_toplevel_decoration_v1_listener WaylandApi::Private::zxdgToplevelDecorationV1Listener = {
  .configure = onZxdgToplevelDecorationV1Configure,
  };

const wp_fractional_scale_v1_listener WaylandApi::Private::wpFractionalScaleV1Listener = {
  .preferred_scale = onWpFractionalScaleV1PreferredScale,
  };

// Frame callbacks (wl_surface.frame)
const wl_callback_listener WaylandApi::Private::wlCallbackListener = {
  .done = onWlCallbackDone,
  };

const zwp_locked_pointer_v1_listener WaylandApi::Private::zwpLockedPointerV1Listener = {
  .locked   = onZwpLockedPointerV1Locked,
  .unlocked = onZwpLockedPointerV1Unlocked,
  };

// Connection

bool WaylandApi::Private::connect() {
  // True only if Wayland is usable (connected + all required globals); otherwise logs why.
  // Note: ::dpy set in constructor/destructor only!

  // Manual override, e.g. to test the X11 path: TEMPEST_DISABLE_WAYLAND=1
  const char* disable = std::getenv("TEMPEST_DISABLE_WAYLAND");
  if(disable!=nullptr && std::strcmp(disable,"1")==0) {
    Log::i("WaylandApi: Wayland disabled by TEMPEST_DISABLE_WAYLAND=1");
    return false;
    }

  // TODO(wayland): WAYLAND_DISPLAY check: wl_display_connect(nullptr) falls back to "wayland-0",
  // which may belong to another session of the same user (e.g. X11 session here, Wayland session
  // on another Virtual Terminal).

  display = wl_display_connect(nullptr);
  if(!display) {
    const int   err  = errno;
    const char* name = std::getenv("WAYLAND_DISPLAY");
    Log::i("WaylandApi: can't connect to a compositor (WAYLAND_DISPLAY=", (name!=nullptr ? name : "not set"), "): ",
           std::strerror(err));
    return false;
    }

  // Needed to compile keymap: without it there's no keyboard input
  xkbContext = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
  if(!xkbContext) {
    Log::i("WaylandApi: xkb_context_new failed");
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

void WaylandApi::Private::disconnect() {
  // Handles partial state and is safe to call twice (constructor on failure, destructor).
  // Connection errors are ignored while tearing down.
  if(!display)
    return;

  // Windows first: xdg_wm_base must not be destroyed while xdg_surfaces exist ('defunct_surfaces' error).
  while(!windows.empty())
    destroyWindow(windows.back().get());

  if(relativePointerManager) {
    zwp_relative_pointer_manager_v1_destroy(relativePointerManager);
    relativePointerManager = nullptr;
    }

  if(pointerConstraints) {
    zwp_pointer_constraints_v1_destroy(pointerConstraints);
    pointerConstraints = nullptr;
    }

  if(cursorShapeManager) {
    wp_cursor_shape_manager_v1_destroy(cursorShapeManager);
    cursorShapeManager = nullptr;
    }

  if(fractionalScaleManager) {
    wp_fractional_scale_manager_v1_destroy(fractionalScaleManager);
    fractionalScaleManager = nullptr;
    }

  if(viewporter) {
    wp_viewporter_destroy(viewporter);
    viewporter = nullptr;
    }

  if(decorationManager) {
    zxdg_decoration_manager_v1_destroy(decorationManager);
    decorationManager = nullptr;
    }

  if(wmBase) {
    xdg_wm_base_destroy(wmBase);
    wmBase = nullptr;
    }

  // Seat devices before the seat
  releaseKeyboard();
  releasePointer();

  if(xkbState) {
    xkb_state_unref(xkbState);
    xkbState = nullptr;
    }
  if(xkbKeymap) {
    xkb_keymap_unref(xkbKeymap);
    xkbKeymap = nullptr;
    }
  if(xkbContext) {
    xkb_context_unref(xkbContext);
    xkbContext = nullptr;
    }

  if(seat) {
    static_assert(wlSeatVersion>=WL_SEAT_RELEASE_SINCE_VERSION);
    wl_seat_release(seat);
    seat = nullptr;
    }

  if(compositor) {
    wl_compositor_destroy(compositor);
    compositor = nullptr;
    }

  if(registry) {
    wl_registry_destroy(registry);
    registry = nullptr;
    }

  wl_display_flush(display);
  wl_display_disconnect(display);
  display = nullptr;
  }

bool WaylandApi::Private::hasRequiredGlobals() const {
  // No fallbacks yet without fractional-scale, cursor-shape or server-side decorations (e.g. GNOME uses X11 for now).
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
    Log::i("WaylandApi: compositor doesn't offer ", r.interface.name, " version ", r.version, " or newer");
    hasAll = false;
    }
  return hasAll;
  }

void WaylandApi::Private::releasePointer() {
  // release what the app saw pressed while pointer still exists
  if(pointerFocus!=nullptr)
    releaseHeldButtons(*pointerFocus);
  // Objects created from the pointer first.
  for(auto& w:windows)
    setPointerLock(*w, false);
  if(relativePointer) {
    zwp_relative_pointer_v1_destroy(relativePointer);
    relativePointer = nullptr;
    }
  if(cursorShapeDevice) {
    wp_cursor_shape_device_v1_destroy(cursorShapeDevice);
    cursorShapeDevice = nullptr;
    }
  if(pointer) {
    wl_pointer_release(pointer);
    pointer = nullptr;
    }
  pointerFocus = nullptr;
  heldButtons.clear();
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
    zwp_locked_pointer_v1_destroy(w.lockedPointer);
    w.lockedPointer = nullptr;
    w.pointerLocked = false;
    }
  }

void WaylandApi::Private::releaseKeyboard() {
  // release what the app saw pressed while keyboard still exists
  if(keyboardFocus!=nullptr)
    releaseHeldKeys(*keyboardFocus);
  if(keyboard) {
    wl_keyboard_release(keyboard);
    keyboard = nullptr;
    }
  keyboardFocus = nullptr;
  repeatKeycode = 0;
  heldKeys.clear();
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

// Windows

WWindow* WaylandApi::Private::createWindow(Tempest::Window* owner, uint32_t width, uint32_t height, SystemApi::ShowMode sm) {
  // all roles and state requests needed before the first commit
  windows.emplace_back(std::make_unique<WWindow>());
  WWindow* win = windows.back().get();
  win->owner              = owner;
  win->appRequestedWidth  = int32_t(width);
  win->appRequestedHeight = int32_t(height);

  win->surface = wl_compositor_create_surface(compositor);
  wl_surface_add_listener(win->surface, &wlSurfaceListener, win);

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

  // Window's constructor reads the client rect itself after this returns: no SizeEvent for this size.
  win->dispatchedWidth  = win->physicalWidth();
  win->dispatchedHeight = win->physicalHeight();
  return win;
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

  for(auto cb:w->frameCallbacks)
    wl_callback_destroy(cb);
  w->frameCallbacks.clear();
  setPointerLock(*w, false); // created for the wl_surface
  if(w->fractionalScale) {
    wp_fractional_scale_v1_destroy(w->fractionalScale);
    w->fractionalScale = nullptr;
    }
  if(w->viewport) {
    wp_viewport_destroy(w->viewport);
    w->viewport = nullptr;
    }
  if(w->decoration) { // before toplevel
    zxdg_toplevel_decoration_v1_destroy(w->decoration);
    w->decoration = nullptr;
    }
  if(w->toplevel) {
    xdg_toplevel_destroy(w->toplevel);
    w->toplevel = nullptr;
    }
  if(w->xdgSurface) {
    xdg_surface_destroy(w->xdgSurface);
    w->xdgSurface = nullptr;
    }
  if(w->surface) {
    wl_surface_destroy(w->surface);
    w->surface = nullptr;
    }

  // After deletion events still queued for its objects are dropped by libwayland
  std::erase_if(windows, [w](const std::unique_ptr<WWindow>& p) { return p.get()==w; });
  }

void WaylandApi::Private::applyConfigure(WWindow& w, uint32_t serial) {
  // pending becomes current and is acked
  if(w.pending.width==0 || w.pending.height==0) {
    // client decides: app requested size needs conversion to logical (KWin sends
    // preferred_scale before the first configure).
    w.pending.width  = std::max(1, int32_t(std::lround(float(w.appRequestedWidth) /w.scale())));
    w.pending.height = std::max(1, int32_t(std::lround(float(w.appRequestedHeight)/w.scale())));
    }
  w.width      = w.pending.width;
  w.height     = w.pending.height;
  w.fullscreen = w.pending.fullscreen;

  // Viewport state is double-buffered: it applies with the next commit, which Vulkan does in vkQueuePresent.
  wp_viewport_set_destination(w.viewport, w.width, w.height);

  xdg_surface_ack_configure(w.xdgSurface, serial);
  w.configured = true;
  }

void WaylandApi::Private::updateState(WWindow& w) {
  // Update things that should change only once per draw, not on every triggered callback (e.g. swapchain resize)
  const int32_t pw = w.physicalWidth();
  const int32_t ph = w.physicalHeight();
  if(pw!=w.dispatchedWidth || ph!=w.dispatchedHeight) {
    w.dispatchedWidth  = pw;
    w.dispatchedHeight = ph;
    SizeEvent e(pw, ph);
    SystemApi::dispatchResize(*w.owner, e);
    }
  }

// Input

MouseEvent WaylandApi::Private::pointerEvent(const WWindow& w, Event::MouseButton button, int delta, Event::Type type) {
  // floor: the physical pixel the cursor is in.
  const int x = int(std::floor(w.cursorX));
  const int y = int(std::floor(w.cursorY));
  return MouseEvent(x, y, button, Event::M_NoModifier, delta, 0, type);
  }

bool WaylandApi::Private::isWindowFocused(const WWindow* w) const {
  // Window focus = keyboard focus: mouse moves, wheel and presses only reach the focused window.
  // Without a keyboard every window counts as focused (pointer events only come for the one under the pointer).
  return keyboard==nullptr || keyboardFocus==w;
  }

void WaylandApi::Private::releaseHeldInput(WWindow& w) {
  // On focus loss the real releases never reach the app, so keys and buttons would stay held (e.g. Alt after Alt+Tab).
  releaseHeldKeys(w);
  releaseHeldButtons(w);
  }

void WaylandApi::Private::releaseHeldKeys(WWindow& w) {
  repeatKeycode = 0;
  auto keys = std::move(heldKeys);
  heldKeys.clear();
  if(w.owner==nullptr)
    return;
  for(uint32_t keycode:keys) {
    KeyEvent e = keyEvent(keycode, Event::KeyUp);
    SystemApi::dispatchKeyUp(*w.owner, e, keycode);
    }
  }

void WaylandApi::Private::releaseHeldButtons(WWindow& w) {
  auto buttons = std::move(heldButtons);
  heldButtons.clear();
  if(w.owner==nullptr)
    return;
  for(uint32_t button:buttons) {
    MouseEvent e = pointerEvent(w, toButton(button), 0, Event::MouseUp);
    SystemApi::dispatchMouseUp(*w.owner, e);
    }
  }

KeyEvent WaylandApi::Private::keyEvent(uint32_t keycode, Event::Type type) const {
  // keycode: xkb keycode (evdev + 8). key: level-0 keysym of the active layout, so Shift+W stays K_W (like Windows'
  // virtual keys). code: the typed character, 0 if none (e.g. arrows, dead keys).
  const xkb_layout_index_t layout = xkb_state_key_get_layout(xkbState, keycode);
  const xkb_keysym_t*      syms   = nullptr;
  const int                count  = xkb_keymap_key_get_syms_by_level(xkbKeymap, keycode, layout, 0, &syms);
  const xkb_keysym_t       sym    = count>0 ? syms[0] : XKB_KEY_NoSymbol;
  const auto               key    = Event::KeyType(SystemApi::translateKey(sym));

  // Only characters up to U+FFFF (Basic Multilingual Plane): Tempest's text input can't hold more.
  // Beyond it, nothing is typed (X11 and Windows deliver half a UTF-16 surrogate pair there).
  const uint32_t utf32 = xkb_state_key_get_utf32(xkbState, keycode);
  const uint32_t code  = utf32<=0xFFFF ? utf32 : 0;
  return KeyEvent(key, code, Event::M_NoModifier, type);
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
  KeyEvent e = keyEvent(repeatKeycode, Event::KeyDown);
  SystemApi::dispatchKeyDown(*keyboardFocus->owner, e, repeatKeycode);
  }

// Event loop

void WaylandApi::Private::readEvents() {
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
  // Timeout computed after dispatching the queued events: a frame callback among them may let a window render now.
  if(poll(&fd, 1, pollTimeout())>0) {
    if(wl_display_read_events(display)<0) {
      handleConnectionError();
      return;
      }
    } else {
    wl_display_cancel_read(display);
    }

  if(wl_display_dispatch_pending(display)<0)
    handleConnectionError();
  }

void WaylandApi::Private::handleConnectionError() {
  // connection dead, nothing works anymore, end the app
  logDisplayError();
  SystemApi::exit();
  }

int WaylandApi::Private::pollTimeout() const {
  // 0 if a window may render; otherwise wait for the compositor (e.g. a frame callback),
  // at most idlePollTimeoutMs, and not beyond the next scheduled key repeat
  int timeout = hasWindowToRender() ? 0 : idlePollTimeoutMs;
  if(repeatKeycode!=0) {
    const auto untilRepeat = std::chrono::ceil<std::chrono::milliseconds>(repeatNext - std::chrono::steady_clock::now());
    timeout = std::clamp(int(untilRepeat.count()), 0, timeout);
    }
  return timeout;
  }

bool WaylandApi::Private::hasWindowToRender() const {
  for(auto& w:windows)
    if(w->readyToRender())
      return true;
  return false;
  }

void WaylandApi::Private::renderWindows() {
  // Skips windows waiting for frame callbacks: nothing is drawn while hidden, and we never present into a FIFO
  // swapchain the compositor doesn't consume (could block forever).
  for(auto& w:windows) {
    if(!w->readyToRender())
      continue;
    updateState(*w);
    SystemApi::dispatchRender(*w->owner);
    }
  }

// Session listeners

void WaylandApi::Private::onWlRegistryGlobal(void* data, wl_registry* registry, uint32_t name, const char* interface, uint32_t version) {
  // Binds exactly the *Version constants; older globals are skipped.
  WaylandApi::Private* self = static_cast<Private*>(data);
  if(std::strcmp(interface, wl_compositor_interface.name)==0 && version>=wlCompositorVersion) {
    self->compositor = static_cast<wl_compositor*>(wl_registry_bind(registry, name, &wl_compositor_interface, wlCompositorVersion));
    }
  if(std::strcmp(interface, xdg_wm_base_interface.name)==0 && version>=xdgWmBaseVersion) {
    static_assert(xdgWmBaseVersion<XDG_TOPLEVEL_CONFIGURE_BOUNDS_SINCE_VERSION,
                  "xdgToplevelListener needs configure_bounds/wm_capabilities handlers at this version");
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
  static_assert(wlSeatVersion>=WL_POINTER_RELEASE_SINCE_VERSION && wlSeatVersion>=WL_KEYBOARD_RELEASE_SINCE_VERSION);
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
  // Like X11's EnterNotify: a move to the entry position, then re-apply the window's cursor (required by Wayland)
  auto self = static_cast<Private*>(data);
  if(surface==nullptr)
    return; // surface was destroyed after compositor sent the event
  auto w = static_cast<WWindow*>(wl_surface_get_user_data(surface)); // listener data of our wl_surface
  self->pointerFocus       = w;
  self->pointerEnterSerial = serial;
  // logical -> physical
  w->cursorX               = wl_fixed_to_double(x)*w->scale();
  w->cursorY               = wl_fixed_to_double(y)*w->scale();
  if(w->owner==nullptr)
    return;
  if(self->isWindowFocused(w)) {
    MouseEvent e = pointerEvent(*w, Event::ButtonNone, 0, Event::MouseMove);
    SystemApi::dispatchMouseMove(*w->owner, e);
    }
  // Note: unfocused window gets desktop cursor (see implShowCursor)
  SystemApi::showCursor(reinterpret_cast<SystemApi::Window*>(w), SystemApi::cursorShape(*w->owner));
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
  w->cursorX = wl_fixed_to_double(x)*w->scale();
  w->cursorY = wl_fixed_to_double(y)*w->scale();
  if(w->owner==nullptr || !self->isWindowFocused(w))
    return;
  MouseEvent e = pointerEvent(*w, Event::ButtonNone, 0, Event::MouseMove);
  SystemApi::dispatchMouseMove(*w->owner, e);
  }

void WaylandApi::Private::onWlPointerButton(void* data, [[maybe_unused]] wl_pointer* pointer, [[maybe_unused]] uint32_t serial,
                                            [[maybe_unused]] uint32_t time, uint32_t button, uint32_t state) {
  // Unknown buttons become ButtonNone, like X11. Releases only for presses the app got (see heldButtons).
  auto self = static_cast<Private*>(data);
  WWindow* w = self->pointerFocus;
  if(w==nullptr || w->owner==nullptr)
    return;

  if(state==WL_POINTER_BUTTON_STATE_PRESSED) {
    if(!self->isWindowFocused(w))
      return;
    // Focus click filter: the click that only focuses the window must not reach the app. KWin sends
    // wl_keyboard.enter and that press together, so a press in the same readEvents() batch as the enter is that click.
    if(self->focusGainedInBatch==w)
      return;
    if(std::find(self->heldButtons.begin(), self->heldButtons.end(), button)==self->heldButtons.end())
      self->heldButtons.push_back(button);
    MouseEvent e = pointerEvent(*w, toButton(button), 0, Event::MouseDown);
    SystemApi::dispatchMouseDown(*w->owner, e);
    } else {
    if(std::erase(self->heldButtons, button)==0)
      return;
    MouseEvent e = pointerEvent(*w, toButton(button), 0, Event::MouseUp);
    SystemApi::dispatchMouseUp(*w->owner, e);
    }
  }

void WaylandApi::Private::onWlPointerAxisValue120(void* data, [[maybe_unused]] wl_pointer* pointer, uint32_t axis, int32_t value120) {
  // 120 per notch, like Tempest's delta on Windows; Wayland's sign is inverted (positive = down).
  // Horizontal scrolling is ignored, like on X11.
  auto self = static_cast<Private*>(data);
  if(axis!=WL_POINTER_AXIS_VERTICAL_SCROLL)
    return;
  if(self->pointerFocus==nullptr || self->pointerFocus->owner==nullptr || !self->isWindowFocused(self->pointerFocus))
    return;
  MouseEvent e = pointerEvent(*self->pointerFocus, Event::ButtonNone, -value120, Event::MouseWheel);
  SystemApi::dispatchMouseWheel(*self->pointerFocus->owner, e);
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
  w->cursorX = std::clamp(w->cursorX + wl_fixed_to_double(dxUnaccel)*s, 0.0, double(std::max(w->physicalWidth() -1, 0)));
  w->cursorY = std::clamp(w->cursorY + wl_fixed_to_double(dyUnaccel)*s, 0.0, double(std::max(w->physicalHeight()-1, 0)));
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

  if(self->xkbState)
    xkb_state_unref(self->xkbState);
  if(self->xkbKeymap)
    xkb_keymap_unref(self->xkbKeymap);
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
  FocusEvent e(true, Event::UnknownReason);
  SystemApi::dispatchFocus(*w->owner, e);
  // Pointer already over the window: like an enter, a move to its position and the app's cursor.
  if(self->pointerFocus==w) {
    MouseEvent m = pointerEvent(*w, Event::ButtonNone, 0, Event::MouseMove);
    SystemApi::dispatchMouseMove(*w->owner, m);
    SystemApi::showCursor(reinterpret_cast<SystemApi::Window*>(w), SystemApi::cursorShape(*w->owner));
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
  self->releaseHeldInput(*w);
  self->keyboardFocus = nullptr;
  if(w->owner==nullptr)
    return;
  FocusEvent e(false, Event::UnknownReason);
  SystemApi::dispatchFocus(*w->owner, e);
  if(self->pointerFocus==w)
    SystemApi::showCursor(reinterpret_cast<SystemApi::Window*>(w), SystemApi::cursorShape(*w->owner));
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
  const uint32_t keycode = key + 8;
  if(state==WL_KEYBOARD_KEY_STATE_PRESSED) {
    if(std::find(self->heldKeys.begin(), self->heldKeys.end(), keycode)==self->heldKeys.end())
      self->heldKeys.push_back(keycode);
    KeyEvent e = self->keyEvent(keycode, Event::KeyDown);
    SystemApi::dispatchKeyDown(*self->keyboardFocus->owner, e, keycode);
    self->startKeyRepeat(keycode);
    } else {
    if(std::erase(self->heldKeys, keycode)==0)
      return;
    if(keycode==self->repeatKeycode)
      self->repeatKeycode = 0;
    KeyEvent e = self->keyEvent(keycode, Event::KeyUp);
    SystemApi::dispatchKeyUp(*self->keyboardFocus->owner, e, keycode);
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

// Window specific listeners

void WaylandApi::Private::onXdgSurfaceConfigure(void* data, [[maybe_unused]] xdg_surface* xdgSurface, uint32_t serial) {
  // End of configure sequence (after xdg_toplevel.configure etc.)
  applyConfigure(*static_cast<WWindow*>(data), serial);
  }

void WaylandApi::Private::onXdgToplevelConfigure(void* data, [[maybe_unused]] xdg_toplevel* toplevel, int32_t width, int32_t height, wl_array* states) {
  // Only fills w->pending; it becomes current in applyConfigure.
  auto w = static_cast<WWindow*>(data);
  // Logical size; 0 = client decides, keep the previous size
  if(width>0 && height>0) {
    w->pending.width  = width;
    w->pending.height = height;
    }
  // 'states': the complete list of xdg_toplevel_state values
  w->pending.fullscreen = false;
  auto state = static_cast<const uint32_t*>(states->data);
  for(size_t i=0; i<states->size/sizeof(uint32_t); ++i) {
    if(state[i]==XDG_TOPLEVEL_STATE_FULLSCREEN)
      w->pending.fullscreen = true;
    }
  }

void WaylandApi::Private::onXdgToplevelClose(void* data, [[maybe_unused]] xdg_toplevel* toplevel) {
  // Only a request. Like the Windows backend: the app or an overlay may accept the CloseEvent; otherwise exit.
  auto w = static_cast<WWindow*>(data);
  if(w->owner==nullptr)
    return;
  CloseEvent e;
  SystemApi::dispatchClose(*w->owner, e);
  if(!e.isAccepted())
    SystemApi::exit();
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

void WaylandApi::Private::onWlCallbackDone(void* data, wl_callback* callback, [[maybe_unused]] uint32_t time) {
  // The compositor has shown the content (all callbacks up to it fire together).
  auto w  = static_cast<WWindow*>(data);
  auto it = std::find(w->frameCallbacks.begin(), w->frameCallbacks.end(), callback);
  if(it!=w->frameCallbacks.end())
    w->frameCallbacks.erase(it);
  wl_callback_destroy(callback);
  }

// WaylandApi

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
  if(impl->connect())
    dpy = impl->display; // only here does Wayland become active backend
  else
    impl->disconnect();  // leave nothing half-connected; SystemApi::inst() falls back to X11
  }

WaylandApi::~WaylandApi() {
  dpy = nullptr;
  impl->disconnect();
  impl.reset();
  }

wl_display* WaylandApi::display() {
  SystemApi::inst();
  return dpy;
  }

wl_surface* WaylandApi::surface(SystemApi::Window* w) {
  if(dpy==nullptr || w==nullptr)
    return nullptr;
  return toWWindow(w)->surface;
  }

void WaylandApi::preparePresent(SystemApi::Window* w, uint32_t maxFramesAhead) {
  // The frame request is double-buffered wl_surface state, sent by the driver's commit in vkQueuePresentKHR. Requested
  // here, so every request gets a commit: a render without a present can't leave the window waiting forever.
  // Note: Since this can be called without a Wayland API active, we need to be extra careful
  if(dpy==nullptr || w==nullptr)
    return;
  auto ww = toWWindow(w);
  if(ww->maxFramesAhead!=maxFramesAhead) {
    ww->maxFramesAhead = maxFramesAhead;
    Log::i("WaylandApi: up to ", maxFramesAhead, " presents ahead of the compositor");
    }
  // Always a new callback, even if the list is full: one per present keeps the count exact.
  auto cb = wl_surface_frame(ww->surface);
  wl_callback_add_listener(cb, &Private::wlCallbackListener, ww);
  ww->frameCallbacks.push_back(cb);
  }

void WaylandApi::presentFailed(SystemApi::Window* w) {
  // The driver may have failed before committing the callback
  // unknown state -> delete to avoid locking render loop
  if(dpy==nullptr || w==nullptr)
    return;
  auto ww = toWWindow(w);
  if(ww->frameCallbacks.empty())
    return;
  wl_callback_destroy(ww->frameCallbacks.back());
  ww->frameCallbacks.pop_back();
  }

void WaylandApi::request() {
  requested.store(true);
  }

bool WaylandApi::isRequested() {
  return requested.load();
  }

bool WaylandApi::isConnected() const {
  // Alternative to display() for checking active Wayland backend
  // to avoid circular dependency inside SystemApi::inst()
  // display()->SystemApi::inst()->isConnected()
  return dpy!=nullptr;
  }

SystemApi::Window* WaylandApi::implCreateWindow(Tempest::Window* owner, uint32_t width, uint32_t height) {
  return reinterpret_cast<SystemApi::Window*>(impl->createWindow(owner,width,height,SystemApi::Normal));
  }

SystemApi::Window* WaylandApi::implCreateWindow(Tempest::Window* owner, ShowMode sm) {
  // 800x600 like the other backends; Maximized/FullScreen get their size from the first configure.
  // Hidden treated as Normal.
  return reinterpret_cast<SystemApi::Window*>(impl->createWindow(owner,800,600,sm));
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
  ww->cursorX = x;
  ww->cursorY = y;
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
    wp_cursor_shape_device_v1_set_shape(impl->cursorShapeDevice, impl->pointerEnterSerial, toCursorShape(shape));
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
  // Unlike X11, handles all pending events and then renders, so a steady stream of events can't starve rendering.
  // readEvents() only waits while no window may render (see pollTimeout()).
  impl->readEvents();
  impl->processKeyRepeat();
  if(cb.onTimer()==0)
    std::this_thread::yield();
  impl->renderWindows();
  }

#endif
