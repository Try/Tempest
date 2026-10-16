#include "waylandapi.h"

#if defined(TEMPEST_BUILD_WAYLAND)
#include <Tempest/Event>
#include <Tempest/Window>
#include <Tempest/Log>

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <thread>
#include <vector>

#include <poll.h>

#include <wayland-client.h>

#include "xdg-shell-client-protocol.h"
#include "xdg-decoration-unstable-v1-client-protocol.h"
#include "viewporter-client-protocol.h"
#include "fractional-scale-v1-client-protocol.h"

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

  int32_t  appRequestedWidth  = 0; // physical; used until the compositor sends a size
  int32_t  appRequestedHeight = 0;

  // size the app last saw; updateState() dispatches a resize when it differs.
  int32_t  dispatchedWidth  = 0; // physical
  int32_t  dispatchedHeight = 0;

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
///   SESSION STATE
//

struct WaylandApi::Private {
  wl_display*                      display                = nullptr;
  wl_registry*                     registry               = nullptr;

  // Globals
  wl_compositor*                   compositor             = nullptr;
  xdg_wm_base*                     wmBase                 = nullptr;
  zxdg_decoration_manager_v1*      decorationManager      = nullptr;
  wp_viewporter*                   viewporter             = nullptr;
  wp_fractional_scale_manager_v1*  fractionalScaleManager = nullptr;

  // Exact versions we need to bind successfully: listeners and requests are written for these
  static constexpr uint32_t wlCompositorVersion               = 1;
  static constexpr uint32_t xdgWmBaseVersion                  = 6; // v6: xdg_toplevel state 'suspended'
  static constexpr uint32_t zxdgDecorationManagerV1Version    = 1;
  static constexpr uint32_t wpViewporterVersion               = 1;
  static constexpr uint32_t wpFractionalScaleManagerV1Version = 1;

  std::vector<std::unique_ptr<WWindow>> windows; // owns the windows; each WWindow keeps its address (handle, listener data)

  // Session listeners. 'data' is Private*.
  static void onWlRegistryGlobal(void* data, wl_registry* registry, uint32_t name, const char* interface, uint32_t version);
  static void onXdgWmBasePing(void* data, xdg_wm_base* wmBase, uint32_t serial);

  // Window specific listeners. 'data' is WWindow*.
  static void onXdgSurfaceConfigure(void* data, xdg_surface* xdgSurface, uint32_t serial);
  static void onXdgToplevelConfigure(void* data, xdg_toplevel* toplevel, int32_t width, int32_t height, wl_array* states);
  static void onXdgToplevelClose    (void* data, xdg_toplevel* toplevel);
  static void onZxdgToplevelDecorationV1Configure(void* data, zxdg_toplevel_decoration_v1* decoration, uint32_t mode);
  static void onWpFractionalScaleV1PreferredScale(void* data, wp_fractional_scale_v1* fractionalScale, uint32_t scale);

  // Listener tables (session)
  static const wl_registry_listener                 wlRegistryListener;
  static const xdg_wm_base_listener                 xdgWmBaseListener;

  // Listener tables (window specific)
  static const wl_surface_listener                  wlSurfaceListener;
  static const xdg_surface_listener                 xdgSurfaceListener;
  static const xdg_toplevel_listener                xdgToplevelListener;
  static const zxdg_toplevel_decoration_v1_listener zxdgToplevelDecorationV1Listener;
  static const wp_fractional_scale_v1_listener      wpFractionalScaleV1Listener;

  // Helpers

  static constexpr auto         ioIgnore = [](auto...) {}; // useful to silence Wayland listener stubs
  template<class T> static void ioDestroy(T*& obj, void (*destroyFn)(T*));
  void                          logDisplayError() const;

  // Connection and windows
  bool               hasRequiredGlobals() const;
  bool               connect();
  void               disconnect();
  SystemApi::Window* createWindow(Tempest::Window* owner, uint32_t width, uint32_t height, SystemApi::ShowMode sm);
  void               destroyWindow(WWindow* w);

  // Event loop
  void        handleConnectionError();
  int         pollTimeout() const;
  void        readEvents();
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
  // [input]
  }

void WaylandApi::Private::onXdgWmBasePing([[maybe_unused]] void* data, xdg_wm_base* wmBase, uint32_t serial) {
  xdg_wm_base_pong(wmBase, serial);
  }

///     Window specific listeners
//
void WaylandApi::Private::onXdgSurfaceConfigure(void* data, [[maybe_unused]] xdg_surface* xdgSurface, uint32_t serial) {
  // End of configure sequence (after xdg_toplevel.configure etc.): pending becomes current and is acked
  auto w = static_cast<WWindow*>(data);
  if(w->pending.width==0 || w->pending.height==0) {
    // pending is 0 only if the compositor never sent a size (first configure)
    // client decides: app requested size needs conversion to logical (KWin sends preferred_scale before the first configure).
    w->pending.width  = std::max(1, int32_t(std::lround(float(w->appRequestedWidth) /w->scale())));
    w->pending.height = std::max(1, int32_t(std::lround(float(w->appRequestedHeight)/w->scale())));
    }
  w->width      = w->pending.width;
  w->height     = w->pending.height;
  w->fullscreen = w->pending.fullscreen;
  w->suspended  = w->pending.suspended;

  // Viewport state is double-buffered: it applies with the next commit, which Vulkan does in vkQueuePresent.
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

////
///     CONNECTION AND WINDOWS
//

bool WaylandApi::Private::hasRequiredGlobals() const {
  // No fallbacks yet without fractional-scale or server-side decorations (e.g. GNOME doesn't work).
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
    // [input] {seat,                   wl_seat_interface,                         wlSeatVersion                     },
    // [input] {cursorShapeManager,     wp_cursor_shape_manager_v1_interface,      wpCursorShapeManagerV1Version     },
    // [input] {pointerConstraints,     zwp_pointer_constraints_v1_interface,      zwpPointerConstraintsV1Version    },
    // [input] {relativePointerManager, zwp_relative_pointer_manager_v1_interface, zwpRelativePointerManagerV1Version},
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

  // [input] // Needed to compile keymap: without it there's no keyboard input
  // [input] xkbContext = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
  // [input] if(!xkbContext) {
    // [input] Log::e("WaylandApi: xkb_context_new failed");
    // [input] return false;
    // [input] }

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

  // Windows first: xdg_wm_base must not be destroyed while xdg_surfaces exist
  while(!windows.empty())
    destroyWindow(windows.back().get());

  // [input] ioDestroy(relativePointerManager, zwp_relative_pointer_manager_v1_destroy);
  // [input] ioDestroy(pointerConstraints,     zwp_pointer_constraints_v1_destroy);
  // [input] ioDestroy(cursorShapeManager,     wp_cursor_shape_manager_v1_destroy);
  ioDestroy(fractionalScaleManager, wp_fractional_scale_manager_v1_destroy);
  ioDestroy(viewporter,             wp_viewporter_destroy);
  ioDestroy(decorationManager,      zxdg_decoration_manager_v1_destroy);
  ioDestroy(wmBase,                 xdg_wm_base_destroy);

  // [input] // Seat devices before the seat
  // [input] releaseKeyboard();
  // [input] releasePointer();

  // [input] ioDestroy(xkbState,   xkb_state_unref);
  // [input] ioDestroy(xkbKeymap,  xkb_keymap_unref);
  // [input] ioDestroy(xkbContext, xkb_context_unref);

  // [input] static_assert(wlSeatVersion>=WL_SEAT_RELEASE_SINCE_VERSION);
  // [input] ioDestroy(seat,       wl_seat_release);
  ioDestroy(compositor, wl_compositor_destroy);
  ioDestroy(registry,   wl_registry_destroy);

  wl_display_flush(display);
  wl_display_disconnect(display);
  display = nullptr;
  }

SystemApi::Window* WaylandApi::Private::createWindow(Tempest::Window* owner, uint32_t width, uint32_t height, SystemApi::ShowMode sm) {
  // all roles and state requests needed before the first commit
  windows.emplace_back(std::make_unique<WWindow>());
  WWindow* win = windows.back().get();
  win->owner              = owner;
  win->appRequestedWidth  = int32_t(width);
  win->appRequestedHeight = int32_t(height);

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

  // Window's constructor reads the client rect itself after this returns: no SizeEvent for this size.
  win->dispatchedWidth  = win->physicalWidth();
  win->dispatchedHeight = win->physicalHeight();
  return reinterpret_cast<SystemApi::Window*>(win->surface); // wl_surface* native handle
  }

void WaylandApi::Private::destroyWindow(WWindow* w) {
  // Reverse order of creation; also used for a half-created window.
  // The VkSurfaceKHR must be gone before the wl_surface: Tempest destroys the swapchain before the window.
  if(w==nullptr)
    return;

  // [input] // The compositor sends wl_pointer/wl_keyboard.leave for a destroyed surface, but only after we've deleted w.
  // [input] if(pointerFocus==w) {
    // [input] pointerFocus = nullptr;
    // [input] heldButtons.clear();
    // [input] }
  // [input] if(keyboardFocus==w) {
    // [input] keyboardFocus = nullptr;
    // [input] repeatKeycode = 0;
    // [input] heldKeys.clear();
    // [input] heldButtons.clear();
    // [input] }
  // [input] if(focusGainedInBatch==w)
    // [input] focusGainedInBatch = nullptr;

  // [input] setPointerLock(*w, false); // created for the wl_surface
  ioDestroy(w->fractionalScale, wp_fractional_scale_v1_destroy);
  ioDestroy(w->viewport,        wp_viewport_destroy);
  ioDestroy(w->decoration,      zxdg_toplevel_decoration_v1_destroy); // before toplevel
  ioDestroy(w->toplevel,        xdg_toplevel_destroy);
  ioDestroy(w->xdgSurface,      xdg_surface_destroy);
  ioDestroy(w->surface,         wl_surface_destroy);

  // After deletion events still queued for its objects are dropped by libwayland
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

int WaylandApi::Private::pollTimeout() const {
  // 0 if a window may render; otherwise wait for the compositor (e.g. a configure that ends 'suspended'),
  // at most idlePollTimeoutMs
  // [input] (and not beyond the next scheduled key repeat)

  // Longest sleep in poll() while no window may render; short enough to keep Tempest's timers accurate.
  constexpr int idlePollTimeoutMs = 5;
  const bool canRender = std::any_of(windows.begin(), windows.end(),
                                     [](const std::unique_ptr<WWindow>& w) { return w->readyToRender(); });
  int timeout = canRender ? 0 : idlePollTimeoutMs;
  // [input] if(repeatKeycode!=0) {
    // [input] const auto untilRepeat = std::chrono::ceil<std::chrono::milliseconds>(repeatNext - std::chrono::steady_clock::now());
    // [input] timeout = std::clamp(int(untilRepeat.count()), 0, timeout);
    // [input] }
  return timeout;
  }

void WaylandApi::Private::readEvents() {
  // Doesn't block beyond pollTimeout(): Tempest's loop must keep running (timers, rendering).
  // The Vulkan driver reads the same connection on its own queue; prepare_read -> read_events/cancel_read
  // is how several readers share the socket without stealing or blocking each other.
  // [input] focusGainedInBatch = nullptr; // a new batch (see onWlPointerButton)
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
  if(poll(&fd, 1, pollTimeout())>0) {
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

  // [input] processKeyRepeat(); // synthetic key events
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
  // [input] key table + setupKeyTranslate(k,24)

  impl.reset(new Private());
  if(impl->connect()) {
    dpy = impl->display; // display() non-null only while connected
    } else {
    impl->disconnect();  // leave nothing half-connected
    Log::e("WaylandApi: Wayland is not usable (use TEMPEST_DISABLE_WAYLAND=1 for X11)");
    }
  }

WaylandApi::~WaylandApi() {
  dpy = nullptr;
  impl->disconnect();
  impl.reset();
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
  if(dpy==nullptr)
    return nullptr;
  return impl->createWindow(owner,width,height,SystemApi::Normal);
  }

SystemApi::Window* WaylandApi::implCreateWindow(Tempest::Window* owner, ShowMode sm) {
  // 800x600 like the other backends; Maximized/FullScreen get their size from the first configure.
  // Hidden treated as Normal.
  if(dpy==nullptr)
    return nullptr;
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

void WaylandApi::implSetCursorPosition([[maybe_unused]] SystemApi::Window* w, [[maybe_unused]] int x, [[maybe_unused]] int y) {
  // [input]
  }

void WaylandApi::implShowCursor([[maybe_unused]] SystemApi::Window* w, [[maybe_unused]] CursorShape show) {
  // [input]
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
  if(cb.onTimer()==0)
    std::this_thread::yield();
  impl->renderWindows();
  }

#endif // defined(TEMPEST_BUILD_WAYLAND)
