#pragma once

#include <Tempest/SystemApi>

namespace Tempest {

class iOSApi final: SystemApi {
  public:
    using  SystemApi::dispatchRender;
  
  private:
    iOSApi();

    Window*  implCreateWindow(Tempest::Window *owner, uint32_t width, uint32_t height) override;
    Window*  implCreateWindow(Tempest::Window *owner, ShowMode sm) override;
    void     implDestroyWindow(Window* w) override;
    void     implExit() override;

    Rect     implWindowClientRect(SystemApi::Window *w) override;
    bool     implSetAsFullscreen(SystemApi::Window *w, bool fullScreen) override;
    bool     implIsFullscreen(SystemApi::Window *w) override;
    void     implSetPreferredFrameRateRange(SystemApi::Window *w, uint32_t minimum, uint32_t maximum, uint32_t preferred) override;

    void     implSetCursorPosition(SystemApi::Window *w, int x, int y) override;
    void     implShowCursor(SystemApi::Window *w, CursorShape cursor) override;

    bool     implIsRunning() override;
    int      implExec(AppCallBack& cb) override;
    void     implProcessEvents(AppCallBack& cb) override;

    void     implSetWindowTitle(SystemApi::Window *w, const char* utf8) override;

  friend class SystemApi;
  };

}

