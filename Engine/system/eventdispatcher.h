#pragma once

#include <Tempest/Event>
#include <Tempest/SystemApi>

namespace Tempest {

class EventDispatcher final {
  public:
    EventDispatcher();

    void dispatchMouseDown (SystemApi::WindowCallback& wc, Tempest::MouseEvent& event);
    void dispatchMouseUp   (SystemApi::WindowCallback& wc, Tempest::MouseEvent& event);
    void dispatchMouseMove (SystemApi::WindowCallback& wc, Tempest::MouseEvent& event);
    void dispatchMouseWheel(SystemApi::WindowCallback& wc, Tempest::MouseEvent& event);

    void dispatchKeyDown   (SystemApi::WindowCallback& wc, Tempest::KeyEvent&   event);
    void dispatchKeyUp     (SystemApi::WindowCallback& wc, Tempest::KeyEvent&   event);
  };

}

