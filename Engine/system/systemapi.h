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

    static Window*  createWindow(WindowCallback* cb,uint32_t width,uint32_t height);
    static void     destroyWindow(Window* w);

    static uint32_t width (Window* w);
    static uint32_t height(Window* w);
    static Rect     windowClientRect(SystemApi::Window *w);

  private:
    static int      exec();

  friend class Application;
  };

}
