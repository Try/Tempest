#pragma once

#include <Tempest/Window>
#include <Tempest/Event>

#include <unordered_map>

namespace Tempest {

class EventDispatcher final {
  public:
    EventDispatcher();

    void dispatchMouseDown (Widget& wnd, Tempest::MouseEvent& event);
    void dispatchMouseUp   (Widget& wnd, Tempest::MouseEvent& event);
    void dispatchMouseMove (Widget& wnd, Tempest::MouseEvent& event);
    void dispatchMouseWheel(Widget& wnd, Tempest::MouseEvent& event);

    void dispatchKeyDown   (Widget& wnd, Tempest::KeyEvent&   event, uint32_t scancode);
    void dispatchKeyUp     (Widget& wnd, Tempest::KeyEvent&   event, uint32_t scancode);

  private:
    std::shared_ptr<Widget::Ref> implDispatch(Tempest::Widget &w, Tempest::MouseEvent& event);
    void                         implMouseWhell(Widget &w, MouseEvent &event);

    std::shared_ptr<Widget::Ref> implDispatch(Tempest::Widget &w, Tempest::KeyEvent&   event);
    void                         implSetMouseOver(const std::shared_ptr<Widget::Ref>& s);
    void                         implExcMouseOver(Widget *w, Widget *old);

    std::shared_ptr<Widget::Ref> lock(std::weak_ptr<Widget::Ref>& w);

    std::weak_ptr<Widget::Ref>   mouseUp;
    std::weak_ptr<Widget::Ref>   mouseOver;

    std::unordered_map<uint32_t,std::weak_ptr<Widget::Ref>> keyUp;
  };

}

