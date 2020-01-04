#pragma once

#include <Tempest/SystemApi>
#include <Tempest/Widget>

namespace Tempest {

class VectorImage;
class TextureAtlas;

class Window : public Widget {
  public:
    enum ShowMode : uint8_t {
      Minimized,
      Normal,
      Maximized,
      FullScreen
      };

    Window();
    Window( ShowMode sm );
    virtual ~Window();

  protected:
    virtual void render();
    void         dispatchPaintEvent(VectorImage &e,TextureAtlas &ta);

    SystemApi::Window* hwnd() const { return id; }

  private:
    SystemApi::Window* id=nullptr;

  friend class SystemApi;
  };

}
