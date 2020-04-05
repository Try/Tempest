#pragma once

#include <Tempest/Widget>
#include <Tempest/SystemApi>

namespace Tempest {

class UiOverlay : public Tempest::Widget {
  public:
    UiOverlay();
    virtual ~UiOverlay();

    void updateWindow();

  private:
    bool bind(Widget& w);
    bool bind(Window& w);
    void dispatchDestroyWindow(SystemApi::Window* w);

    Window* owner=nullptr;

  friend class EventDispatcher;
  };

}

