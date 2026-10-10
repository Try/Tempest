#pragma once

#include "system/systemapi.h"

// opaque libwayland types
struct wl_display;
struct wl_surface;

namespace Tempest {

class WaylandApi final: SystemApi {
  public:
    // For Vulkan (vulkanapi.cpp, vswapchain.cpp).
    // nullptr / no effect if Wayland is not the active backend
    static wl_display* display();
    static wl_surface* surface(SystemApi::Window* w);

    // Frame callback gates render speed (e.g. minimize detect)
    // needs WSI driver (vkQueuePresentKHR) to commit messages -> only request on guaranteed present call
    static void        preparePresent(SystemApi::Window* w, uint32_t maxFramesAhead);
    // sync if vkQueuePresentKHR failed and may not have committed
    static void        presentFailed(SystemApi::Window* w);

  private:
    struct Private;

    WaylandApi();
    ~WaylandApi();

    bool     isConnected() const;

    Window*  implCreateWindow(Tempest::Window *owner, uint32_t width, uint32_t height) override;
    Window*  implCreateWindow(Tempest::Window *owner, ShowMode sm) override;
    void     implDestroyWindow(Window* w) override;
    void     implExit() override;

    Rect     implWindowClientRect(SystemApi::Window *w) override;

    bool     implSetAsFullscreen(SystemApi::Window *w, bool fullScreen) override;
    bool     implIsFullscreen(SystemApi::Window *w) override;
    float    implUiScale(SystemApi::Window* w) override;

    void     implSetWindowTitle(SystemApi::Window *w, const char* utf8) override;

    void     implSetCursorPosition(SystemApi::Window *w, int x, int y) override;
    void     implShowCursor(SystemApi::Window *w, CursorShape show) override;

    int      implExec(AppCallBack& cb) override;
    void     implProcessEvents(AppCallBack& cb) override;
    bool     implIsRunning() override;

    std::unique_ptr<Private> impl;

  friend class SystemApi;
  };

}
