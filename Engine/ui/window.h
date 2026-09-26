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
      FullScreen,
      };

    Window();
    Window( ShowMode sm );
    ~Window() override;

    // Requests a display callback rate, not a guaranteed rendering rate.
    // Zero restores the system default. Ignored on unsupported platforms.
    void setPreferredFrameRate(uint32_t fps);

    // Rates are limited to the screen's maximum; minimum and preferred are
    // clamped to the resulting range. Zero minimum means 1, zero preferred
    // leaves the choice to the system, and zero maximum restores the default.
    void setPreferredFrameRateRange(uint32_t minimum, uint32_t maximum, uint32_t preferred = 0);

    void setWindowTitle(const char* utf8);

  protected:
    virtual void render();
    using        Widget::dispatchPaintEvent;
    void         dispatchPaintEvent(VectorImage &e,TextureAtlas &ta);
    void         closeEvent       (Tempest::CloseEvent& event) override;

    SystemApi::Window* hwnd() const { return id; }

    void         setCursorPosition(int x, int y);
    void         setCursorPosition(const Point& p);

  private:
    void         implShowCursor(CursorShape s);

    SystemApi::Window* id             = nullptr;
    CursorShape        resolvedCursor = CursorShape::Arrow;

  friend class Widget;
  friend class UiOverlay;
  friend class EventDispatcher;
  friend class SystemApi;
  };

}
