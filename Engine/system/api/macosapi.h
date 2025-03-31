#pragma once

#include <Tempest/SystemApi>

namespace Tempest {

namespace Detail {
class ImplMacOSApi {
  public:
    static void onDisplayLink(void* hwnd);
    static void onDidResize(void* hwnd, void* w);
    static void onDidBecomeKey(void* hwnd, void* w);
    static void onDidResignKey(void* hwnd, void* w);
  };
}

class MacOSApi final: SystemApi {
  private:
    MacOSApi();

    Window*  implCreateWindow(Tempest::Window* owner, uint32_t width, uint32_t height, ShowMode sm);
    Window*  implCreateWindow(Tempest::Window *owner, uint32_t width, uint32_t height) override;
    Window*  implCreateWindow(Tempest::Window *owner, ShowMode sm) override;
    void     implDestroyWindow(Window* w) override;
    void     implExit() override;

    Rect     implWindowClientRect(SystemApi::Window *w) override;
    bool     implSetAsFullscreen(SystemApi::Window *w, bool fullScreen) override;
    bool     implIsFullscreen(SystemApi::Window *w) override;

    void     implSetCursorPosition(SystemApi::Window *w, int x, int y) override;
    void     implShowCursor(SystemApi::Window *w, CursorShape show) override;

    void     implSetWindowTitle(SystemApi::Window *w, const char* utf8) override;
    float    implUiScale(SystemApi::Window* w) override;

    bool     implIsRunning() override;
    int      implExec(AppCallBack& cb) override;
    void     implProcessEvents(AppCallBack& cb) override;

  friend class SystemApi;
  friend class Detail::ImplMacOSApi;
  };

}
