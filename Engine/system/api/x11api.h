#pragma once

#include "system/systemapi.h"

typedef union _XEvent XEvent;

namespace Tempest {

class X11Api final: SystemApi {
  public:
    static void* display();

  private:
    struct Private;

    X11Api();
    ~X11Api();

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

    void     dispatchEvent(XEvent &xev);
    void     alignGeometry(Window *w, Tempest::Window& owner);

    std::unique_ptr<Private> impl;
    SystemApi::ClipboardData clipboard;

    SystemApi::ClipboardData implClipboardData(SystemApi::Window *w) override;

  friend class SystemApi;
  };

}
