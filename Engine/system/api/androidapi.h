#pragma once

#include <Tempest/SystemApi>

namespace Tempest {

class AndroidApi final : SystemApi {
  private:
    AndroidApi() = default;

    static Window* createAndroidWindow(Tempest::Window* owner);
    static void onAppCmd(void* app, int32_t cmd);
    static void updateFocus();
    static void updateWindow();

    Window* implCreateWindow(Tempest::Window* owner, uint32_t width, uint32_t height) override;
    Window* implCreateWindow(Tempest::Window* owner, ShowMode sm) override;
    void    implDestroyWindow(Window* w) override;
    void    implExit() override;

    Rect implWindowClientRect(SystemApi::Window* w) override;
    bool implSetAsFullscreen(SystemApi::Window* w, bool fullScreen) override;
    bool implIsFullscreen(SystemApi::Window* w) override;

    void implSetCursorPosition(SystemApi::Window* w, int x, int y) override;
    void implShowCursor(SystemApi::Window* w, CursorShape cursor) override;

    bool implIsRunning() override;
    int  implExec(AppCallBack& cb) override;
    void implProcessEvents(AppCallBack& cb) override;

    void implSetWindowTitle(SystemApi::Window* w, const char* utf8) override;

  friend class SystemApi;
  };

}
