#include "eventdispatcher.h"

using namespace Tempest;

EventDispatcher::EventDispatcher() {
  }

EventDispatcher::EventDispatcher(Widget &root)
  :customRoot(&root){
  }

void EventDispatcher::dispatchMouseDown(Widget &wnd, MouseEvent &e) {
  mouseUp = implDispatch(wnd,e);
  }

void EventDispatcher::dispatchMouseUp(Widget &/*wnd*/, MouseEvent &e) {
  auto w = lock(mouseUp);
  if(w==nullptr)
    return;
  auto p = mouseUp;
  mouseUp.reset();
  w->widget->mouseUpEvent(e);
  if(!e.isAccepted())
    return;
  if(auto w = p.lock()) {
    // w->widget->setFocus(true);
    }
  }

void EventDispatcher::dispatchMouseMove(Widget &wnd, MouseEvent &e) {
  if(auto w = lock(mouseUp)) {
    MouseEvent e0( e.x,
                   e.y,
                   Event::ButtonNone,
                   0,
                   0,
                   Event::MouseDrag  );
    w->widget->mouseDragEvent(e0);
    if(e0.isAccepted())
      return;
    }

  if(auto w = lock(mouseUp)) {
    MouseEvent e1( e.x,
                   e.y,
                   Event::ButtonNone,
                   0,
                   0,
                   Event::MouseMove  );
    w->widget->mouseMoveEvent(e1);
    if(e.isAccepted()) {
      implSetMouseOver(mouseUp.lock());
      return;
      }
    }

  MouseEvent e1( e.x,
                 e.y,
                 Event::ButtonNone,
                 0,
                 0,
                 Event::MouseMove  );
  auto wptr = implDispatch(wnd,e);
  implSetMouseOver(wptr);
  }

void EventDispatcher::dispatchMouseWheel(Widget &wnd, MouseEvent &e){
  implMouseWhell(wnd,e);
  }

void EventDispatcher::dispatchKeyDown(Widget &wnd, KeyEvent &e, uint32_t scancode) {
  auto& k = keyUp[scancode];
  if(!k.expired())
    return; //TODO: key-repeat
  k = implDispatch(wnd,e);
  }

void EventDispatcher::dispatchKeyUp(Widget &/*wnd*/, KeyEvent &e, uint32_t scancode) {
  auto it = keyUp.find(scancode);
  if(it==keyUp.end())
    return;
  if(auto w = lock((*it).second)){
    keyUp.erase(it);
    w->widget->keyUpEvent(e);
    }
  }

void EventDispatcher::dispatchClose(Widget& wnd, CloseEvent& e) {
  wnd.closeEvent(e);
  }

std::shared_ptr<Widget::Ref> EventDispatcher::implDispatch(Widget& w,MouseEvent &event) {
  Point            pos=event.pos();
  Widget::Iterator it(&w);

  for(;it.hasNext();it.next()) {
    Widget* i=it.get();
    if(i->rect().contains(pos)) {
      MouseEvent ex(event.x - i->x(),
                    event.y - i->y(),
                    event.button,
                    event.delta,
                    event.mouseID,
                    event.type());
      auto ptr = implDispatch(*i,ex);
      if(ex.isAccepted() && it.owner!=nullptr) {
        event.accept();
        return ptr;
        }
      }
    }

  if(it.owner!=nullptr) {
    if(event.type()==Event::MouseDown)
      it.owner->mouseDownEvent(event); else
      it.owner->mouseMoveEvent(event);

    if(event.isAccepted() && it.owner)
      return it.owner->selfReference();
    }
  return nullptr;
  }

void EventDispatcher::implMouseWhell(Widget& w,MouseEvent &event) {
  Point            pos=event.pos();
  Widget::Iterator it(&w);
  for(;it.hasNext();it.next()) {
    Widget* i=it.get();
    if(i->rect().contains(pos)){
      MouseEvent ex(event.x - i->x(),
                    event.y - i->y(),
                    event.button,
                    event.delta,
                    event.mouseID,
                    event.type());
      if(it.owner!=nullptr) {
        implMouseWhell(*i,ex);
        if(ex.isAccepted()) {
          event.accept();
          return;
          }
        }
      }
    }

  if(it.owner!=nullptr)
    it.owner->mouseWheelEvent(event);
  }

std::shared_ptr<Widget::Ref> EventDispatcher::implDispatch(Widget &root, KeyEvent &event) {
  Widget* w = &root;
  while(w->state.focus!=nullptr) {
    w = w->state.focus;
    }
  if(w->wstate.focus) {
    auto ptr = w->selfReference();
    w->keyDownEvent(event);
    if(event.isAccepted())
      return ptr;
    }
  return nullptr;
  }

void EventDispatcher::implSetMouseOver(const std::shared_ptr<Widget::Ref> &wptr) {
  if(wptr!=nullptr) {
    if(auto old = mouseOver.lock()) {
      implExcMouseOver(wptr->widget,old->widget);
      } else {
      implExcMouseOver(wptr->widget,nullptr);
      }
    mouseOver = wptr;
    }
  }

void EventDispatcher::implExcMouseOver(Widget* w, Widget* old) {
  if(w==old)
    return;

  auto* wx = old;
  while(wx!=nullptr) {
    wx->wstate.moveOver = false;
    wx = wx->owner();
    }

  wx = w;
  while(wx!=nullptr) {
    wx->wstate.moveOver = true;
    wx = wx->owner();
    }
  }

std::shared_ptr<Widget::Ref> EventDispatcher::lock(std::weak_ptr<Widget::Ref> &w) {
  auto ptr = w.lock();
  if(ptr==nullptr)
    return nullptr;
  Widget* wx = ptr.get()->widget;
  while(wx->owner()!=nullptr) {
    wx = wx->owner();
    }
  if(dynamic_cast<Window*>(wx) || wx==customRoot)
    return ptr;
  return nullptr;
  }
