#pragma once

#include <cstdint>

namespace Tempest{

class SystemApi {
  private:
    SystemApi();

  public:
    struct Window;

    struct WindowCallback {
      virtual ~WindowCallback()=default;
      virtual void onRender(Window* wx)=0;
      virtual void onResize(Window* wx,uint32_t w,uint32_t h)=0;
      };

    static Window*  createWindow(WindowCallback* cb,uint32_t width,uint32_t height);
    static void     destroyWindow(Window* w);
    static void     exec();

    static uint32_t width (Window* w);
    static uint32_t height(Window* w);
  };

}
