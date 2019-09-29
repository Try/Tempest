#pragma once

#include <Tempest/Platform>
#include <Tempest/Rect>
#include <cstdint>

namespace Tempest {

class MouseEvent;
class KeyEvent;
class Application;

class SystemApi {
  public:
    struct Window;

    struct WindowCallback {
      virtual ~WindowCallback()=default;
      virtual void onRender(Window* wx)=0;
      virtual void onResize(Window* wx,int32_t w,int32_t h)=0;
      virtual void onMouse(MouseEvent& e)=0;
      virtual void onKey  (KeyEvent&   e)=0;
      };

    enum ShowMode : uint8_t {
      Minimized,
      Normal,
      Maximized,
      FullScreen
      };

    virtual ~SystemApi()=default;
    static Window*  createWindow(WindowCallback* cb,uint32_t width,uint32_t height);
    static Window*  createWindow(WindowCallback* cb,ShowMode sm);
    static void     destroyWindow(Window* w);
    static void     exit();

    static uint32_t width (Window* w);
    static uint32_t height(Window* w);
    static Rect     windowClientRect(SystemApi::Window *w);

    static bool     setAsFullscreen(SystemApi::Window *w, bool fullScreen);
    static bool     isFullscreen(SystemApi::Window *w);

    static void     setCursorPosition(int x, int y);
    static void     showCursor(bool show);

  protected:
    struct AppCallBack {
      virtual ~AppCallBack()=default;
      virtual uint32_t onTimer()=0;
      };

    SystemApi();
    virtual Window*  implCreateWindow (WindowCallback* cb,uint32_t width,uint32_t height) = 0;
    virtual Window*  implCreateWindow (WindowCallback* cb,ShowMode sm) = 0;
    virtual void     implDestroyWindow(Window* w) = 0;
    virtual void     implExit() = 0;

    virtual uint32_t implWidth (Window* w) = 0;
    virtual uint32_t implHeight(Window* w) = 0;
    virtual Rect     implWindowClientRect(SystemApi::Window *w) = 0;

    virtual bool     implSetAsFullscreen(SystemApi::Window *w, bool fullScreen) = 0;
    virtual bool     implIsFullscreen(SystemApi::Window *w) = 0;

    virtual void     implSetCursorPosition(int x, int y) = 0;
    virtual void     implShowCursor(bool show) = 0;

    virtual int      implExec(AppCallBack& cb) = 0;

  private:
    static int        exec(AppCallBack& cb);
    static SystemApi& inst();

  friend class Application;
  };

}
