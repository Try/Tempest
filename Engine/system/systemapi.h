#pragma once

#include <Tempest/Rect>
#include <cstdint>

namespace Tempest{

class MouseEvent;
class Application;

class SystemApi {
  private:
    SystemApi();

  public:
    struct Window;

    struct WindowCallback {
      virtual ~WindowCallback()=default;
      virtual void onRender(Window* wx)=0;
      virtual void onResize(Window* wx,int32_t w,int32_t h)=0;
      virtual void onMouse(MouseEvent& e)=0;
      };

    enum ShowMode : uint8_t {
      Minimized,
      Normal,
      Maximized,
      FullScreen
      };

    static Window*  createWindow(WindowCallback* cb,uint32_t width,uint32_t height);
    static Window*  createWindow(WindowCallback* cb,ShowMode sm);
    static void     destroyWindow(Window* w);

    static uint32_t width (Window* w);
    static uint32_t height(Window* w);
    static Rect     windowClientRect(SystemApi::Window *w);

    struct TranslateKeyPair final {
      uint16_t src;
      uint16_t result;
      };

    static uint16_t translateKey(uint64_t scancode);

  protected:
    void setupKeyTranslate(const TranslateKeyPair k[]);

  private:
    struct AppCallBack {
      virtual ~AppCallBack()=default;
      virtual uint32_t onTimer()=0;
      };

    static int      exec(AppCallBack& cb);
    static bool     initWindowClass();
    static bool     implInitWindowClass();

  friend class Application;
  };

}
