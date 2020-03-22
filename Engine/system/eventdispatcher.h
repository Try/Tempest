#pragma once

#include <Tempest/Window>
#include <Tempest/Event>

#include <unordered_map>

namespace Tempest {

class EventDispatcher final {
  public:
    EventDispatcher();
    EventDispatcher(Widget& root);

    void dispatchMouseDown (Widget& wnd, Tempest::MouseEvent& event);
    void dispatchMouseUp   (Widget& wnd, Tempest::MouseEvent& event);
    void dispatchMouseMove (Widget& wnd, Tempest::MouseEvent& event);
    void dispatchMouseWheel(Widget& wnd, Tempest::MouseEvent& event);

    void dispatchKeyDown   (Widget& wnd, Tempest::KeyEvent&   event, uint32_t scancode);
    void dispatchKeyUp     (Widget& wnd, Tempest::KeyEvent&   event, uint32_t scancode);
    void dispatchClose     (Widget& wnd, Tempest::CloseEvent& event);

  private:
    std::shared_ptr<Widget::Ref> implDispatch(Tempest::Widget &w, Tempest::MouseEvent& event);
    void                         implMouseWhell(Widget &w, MouseEvent &event);

    bool                         implShortcut(Tempest::Widget &w, Tempest::KeyEvent& event);
    std::shared_ptr<Widget::Ref> implDispatch(Tempest::Widget &w, Tempest::KeyEvent&   event);
    void                         implSetMouseOver(const std::shared_ptr<Widget::Ref>& s, MouseEvent& orig);
    void                         implExcMouseOver(Widget *w, Widget *old);
    void                         handleModKey(const KeyEvent& e);

    std::shared_ptr<Widget::Ref> lock(std::weak_ptr<Widget::Ref>& w);

    Widget*                      customRoot=nullptr;
    std::weak_ptr<Widget::Ref>   mouseUp;
    std::weak_ptr<Widget::Ref>   mouseOver;


    struct Modify final {
      bool ctrlL   = false;
      bool ctrlR   = false;

      bool altL    = false;
      bool altR    = false;

      bool shiftL  = false;
      bool shiftR  = false;
      };
    Modify keyMod;
    KeyEvent::Modifier mkModifier() const;

    std::unordered_map<uint32_t,std::weak_ptr<Widget::Ref>> keyUp;
  };

}

