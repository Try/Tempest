#pragma once

#include <Tempest/SystemApi>
#include <Tempest/Widget>

namespace Tempest {

class VectorImage;
class TextureAtlas;

class Window : public Widget {
  public:
    Window();
    enum ShowMode : uint8_t {
      Minimized,
      Normal,
      Maximized,
      FullScreen
      };
    Window( ShowMode sm );
    virtual ~Window();

    int w() const;
    int h() const;

  protected:
    virtual void render();
    void         dispatchPaintEvent(VectorImage &e,TextureAtlas &ta);

    SystemApi::Window* hwnd() const { return id; }

  private:
    struct Impl:SystemApi::WindowCallback {
      Impl(Window* self):self(self){}
      void onRender(Tempest::SystemApi::Window*) {
        if(self->w()>0 && self->h()>0) {
          self->render();
          }
        }
      void onResize(Tempest::SystemApi::Window*,int32_t w,int32_t h){
        self->resize(w,h);
        }

      void onMouse(MouseEvent& e){
        self->dispatchMoveEvent(e);
        }
      Window* self;
      };

    Impl               impl;
    SystemApi::Window* id=nullptr;
  };

}
