#pragma once

#include "system/systemapi.h"

namespace Tempest {

class WindowsApi final : SystemApi {
  public:
    static uint16_t translateKey(uint64_t scancode);

  private:
    WindowsApi();

    Window*  implCreateWindow(Tempest::Window *owner, uint32_t width, uint32_t height) override;
    Window*  implCreateWindow(Tempest::Window *owner, ShowMode sm) override;
    void     implDestroyWindow(Window* w) override;
    void     implExit() override;

    uint32_t implWidth (Window* w) override;
    uint32_t implHeight(Window* w) override;
    Rect     implWindowClientRect(SystemApi::Window *w) override;

    bool     implSetAsFullscreen(SystemApi::Window *w, bool fullScreen) override;
    bool     implIsFullscreen(SystemApi::Window *w) override;

    void     implSetCursorPosition(int x, int y) override;
    void     implShowCursor(bool show) override;

    int      implExec(AppCallBack& cb) override;

    struct KeyInf;
    struct TranslateKeyPair final {
      uint16_t src;
      uint16_t result;
      };

    static void     setupKeyTranslate(const TranslateKeyPair k[], uint16_t funcCount);
    static long     windowProc(void* hWnd, uint32_t msg, const uint32_t wParam, const long lParam);

    static KeyInf ki;

  friend class SystemApi;
  };

}
