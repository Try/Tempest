#pragma once

#include <Tempest/Platform>
#include <Tempest/Rect>
#include <Tempest/WidgetState>

#include <memory>
#include <cstdint>
#include <vector>

namespace Tempest {

class SizeEvent;
class MouseEvent;
class KeyEvent;
class CloseEvent;
class PaintEvent;
class FocusEvent;

class Widget;
class Window;
class UiOverlay;
class Application;

class SystemApi {
  public:
    struct Window;

    enum ShowMode : uint8_t {
      Minimized,
      Normal,
      Maximized,
      FullScreen,

      Hidden, //internal
      };

    struct TranslateKeyPair final {
      uint16_t src;
      uint16_t result;
      };

    enum class ClipboardDataType : uint8_t {
      Unknown,
      Text,
      };

    struct ClipboardData {
      ClipboardDataType type = ClipboardDataType::Unknown;
      std::vector<uint8_t> data;
      };

    virtual ~SystemApi()=default;
    static Window*  createWindow(Tempest::Window* owner, uint32_t width, uint32_t height);
    static Window*  createWindow(Tempest::Window* owner, ShowMode sm);
    static void     destroyWindow(Window* w);
    static void     exit();

    static Rect     windowClientRect(SystemApi::Window *w);

    static bool     setAsFullscreen(SystemApi::Window *w, bool fullScreen);
    static bool     isFullscreen(SystemApi::Window *w);

    static void     setWindowTitle(SystemApi::Window *w, const char* utf8);
    static float    uiScale(SystemApi::Window *w);

    static uint16_t translateKey(uint64_t scancode);
    static void     setupKeyTranslate(const TranslateKeyPair k[], uint16_t funcCount);

    static void     addOverlay (UiOverlay* ui);
    static void     takeOverlay(UiOverlay* ui);
    static ClipboardData clipboardData(SystemApi::Window *w);

  protected:
    struct AppCallBack {
      virtual ~AppCallBack()=default;
      virtual uint32_t onTimer()=0;
      };

    SystemApi();
    virtual Window*  implCreateWindow (Tempest::Window *owner,uint32_t width,uint32_t height) = 0;
    virtual Window*  implCreateWindow (Tempest::Window *owner,ShowMode sm) = 0;
    virtual void     implDestroyWindow(Window* w) = 0;
    virtual void     implExit() = 0;

    virtual Rect     implWindowClientRect(SystemApi::Window *w) = 0;

    virtual bool     implSetAsFullscreen(SystemApi::Window *w, bool fullScreen) = 0;
    virtual bool     implIsFullscreen(SystemApi::Window *w) = 0;

    virtual void     implSetCursorPosition(SystemApi::Window *w, int x, int y) = 0;
    virtual void     implShowCursor(SystemApi::Window *w, CursorShape show) = 0;

    virtual float    implUiScale(SystemApi::Window* w);

    virtual bool     implIsRunning() = 0;
    virtual int      implExec(AppCallBack& cb) = 0;
    virtual void     implProcessEvents(AppCallBack& cb) = 0;

    virtual void     implSetWindowTitle(SystemApi::Window *w, const char* utf8) = 0;

    virtual ClipboardData implClipboardData(SystemApi::Window *w) { return ClipboardData(); };

    static void      setCursorPosition(SystemApi::Window *w, int x, int y);
    static void      showCursor(SystemApi::Window *w, CursorShape c);

    static void      dispatchOverlayRender(Tempest::Window &w, Tempest::PaintEvent& e);
    static void      dispatchRender    (Tempest::Window& cb);
    static void      dispatchMouseDown (Tempest::Window& cb, MouseEvent& e);
    static void      dispatchMouseUp   (Tempest::Window& cb, MouseEvent& e);
    static void      dispatchMouseMove (Tempest::Window& cb, MouseEvent& e);
    static void      dispatchMouseWheel(Tempest::Window& cb, MouseEvent& e);

    static void      dispatchKeyDown   (Tempest::Window& cb, KeyEvent& e, uint32_t scancode);
    static void      dispatchKeyUp     (Tempest::Window& cb, KeyEvent& e, uint32_t scancode);

    static void      dispatchResize    (Tempest::Window& cb, SizeEvent& e);
    static void      dispatchClose     (Tempest::Window& cb, CloseEvent& e);

    static void      dispatchFocus     (Tempest::Window& cb, FocusEvent& e);

    static SystemApi& inst();

  private:
    static bool       isRunning();
    static int        exec(AppCallBack& cb);
    static void       processEvent(AppCallBack& cb);

    struct Data;
    static Data m;

  friend class Tempest::Window;
  friend class Tempest::Application;
  };

}
