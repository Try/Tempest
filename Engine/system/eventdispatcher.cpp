#include "eventdispatcher.h"

#include <Tempest/Shortcut>
#include <Tempest/Platform>
#include <Tempest/UiOverlay>

using namespace Tempest;

EventDispatcher::EventDispatcher() {
  }

EventDispatcher::EventDispatcher(Widget &root)
  :customRoot(&root){
  }

void EventDispatcher::dispatchMouseDown(Widget &wnd, MouseEvent &e) {
  for(auto i:overlays) {
    if(!i->bind(wnd))
      continue;
    mouseUp = implDispatch(*i,e);
    if(!mouseUp.expired())
      return;
    }
  mouseUp = implDispatch(wnd,e);
  }

void EventDispatcher::dispatchMouseUp(Widget& /*wnd*/, MouseEvent &e) {
  auto ptr = mouseUp;
  mouseUp.reset();

  if(auto w = ptr.lock()) {
    auto p = e.pos() - w->widget->mapToRoot(Point());
    MouseEvent e1( p.x,
                   p.y,
                   e.button,
                   0,
                   0,
                   Event::MouseUp );
    w->widget->mouseUpEvent(e1);
    if(!e1.isAccepted())
      return;
    }

  if(auto w = ptr.lock()) {
    if(w->widget->focusPolicy() & ClickFocus) {
      w->widget->implSetFocus(true,Event::FocusReason::ClickReason);
      }
    }
  }

void EventDispatcher::dispatchMouseMove(Widget &wnd, MouseEvent &e) {
  if(auto w = lock(mouseUp)) {
    auto p = e.pos() - w->widget->mapToRoot(Point());
    MouseEvent e0( p.x,
                   p.y,
                   Event::ButtonNone,
                   0,
                   0,
                   Event::MouseDrag  );
    w->widget->mouseDragEvent(e0);
    if(e0.isAccepted())
      return;
    }

  if(auto w = lock(mouseUp)) {
    auto p = e.pos() - w->widget->mapToRoot(Point());
    MouseEvent e1( p.x,
                   p.y,
                   Event::ButtonNone,
                   0,
                   0,
                   Event::MouseMove  );
    w->widget->mouseMoveEvent(e1);
    if(e.isAccepted()) {
      implSetMouseOver(mouseUp.lock(),e);
      return;
      }
    }

  MouseEvent e1( e.x,
                 e.y,
                 Event::ButtonNone,
                 0,
                 0,
                 Event::MouseMove  );
  for(auto i:overlays) {
    if(!i->bind(wnd))
      continue;
    auto wptr = implDispatch(*i,e);
    if(wptr!=nullptr) {
      implSetMouseOver(wptr,e);
      return;
      }
    }
  auto wptr = implDispatch(wnd,e);
  implSetMouseOver(wptr,e);
  }

void EventDispatcher::dispatchMouseWheel(Widget &wnd, MouseEvent &e){
  for(auto i:overlays) {
    if(!i->bind(wnd))
      continue;
    implMouseWhell(*i,e);
    if(e.isAccepted())
      return;
    }
  implMouseWhell(wnd,e);
  }

void EventDispatcher::dispatchKeyDown(Widget &wnd, KeyEvent &e, uint32_t scancode) {
  auto& k = keyUp[scancode];
  if(auto w = lock(k)) {
    KeyEvent e1(e.key,e.code,mkModifier(),Event::KeyRepeat);
    w->widget->keyRepeatEvent(e1);
    return;
    }

  KeyEvent e1(e.key,e.code,mkModifier(),e.type());
  handleModKey(e);

  if(implShortcut(wnd,e1))
    return;
  k = implDispatch(wnd,e1);
  }

void EventDispatcher::dispatchKeyUp(Widget &/*wnd*/, KeyEvent &e, uint32_t scancode) {
  auto it = keyUp.find(scancode);
  if(it==keyUp.end())
    return;
  KeyEvent e1(e.key,e.code,mkModifier(),e.type());
  handleModKey(e);

  if(auto w = lock((*it).second)){
    keyUp.erase(it);
    w->widget->keyUpEvent(e1);
    }
  }

void EventDispatcher::dispatchResize(Widget& wnd, SizeEvent& e) {
  wnd.resize(int(e.w),int(e.h));
  }

void EventDispatcher::dispatchClose(Widget& wnd, CloseEvent& e) {
  for(auto i:overlays) {
    if(!i->bind(wnd))
      continue;
    i->closeEvent(e);
    if(e.isAccepted()) {
      e.ignore();
      return;
      }
    }
  wnd.closeEvent(e);
  }

void EventDispatcher::dispatchRender(Window& wnd) {
  if(wnd.w()>0 && wnd.h()>0)
    wnd.render();
  }

void EventDispatcher::dispatchOverlayRender(Window& wnd, PaintEvent& e) {
  for(auto i:overlays) {
    if(!i->bind(wnd))
      continue;
    i->dispatchPaintEvent(e);
    }
  }

void EventDispatcher::addOverlay(UiOverlay* ui) {
  overlays.push_back(ui);
  }

void EventDispatcher::takeOverlay(UiOverlay* ui) {
  for(size_t i=0;i<overlays.size();++i)
    if(overlays[i]==ui) {
      overlays.erase(overlays.begin()+int(i));
      return;
      }
  }

void EventDispatcher::dispatchDestroyWindow(SystemApi::Window* w) {
  for(size_t i=0;i<overlays.size();++i)
    overlays[i]->dispatchDestroyWindow(w);
  }

std::shared_ptr<Widget::Ref> EventDispatcher::implDispatch(Widget& w,MouseEvent &event) {
  if(!w.isVisible()) {
    event.ignore();
    return nullptr;
    }
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
  if(!w.isVisible()) {
    event.ignore();
    return;
    }
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

bool EventDispatcher::implShortcut(Widget& w, KeyEvent& event) {
  if(!w.isVisible())
    return false;

  Widget::Iterator it(&w);
  for(;it.hasNext();it.next()) {
    Widget* i=it.get();
    if(implShortcut(*i,event))
      return true;
    }

  std::lock_guard<std::recursive_mutex> guard(Widget::syncSCuts);
  for(auto& sc:w.sCuts) {
    if(sc->key() !=event.key  && sc->key() !=KeyEvent::K_NoKey)
      continue;
    if(sc->lkey()!=event.code && sc->lkey()!=0)
      continue;

    if((sc->modifier()&event.modifier)!=sc->modifier())
      continue;

    sc->onActivated();
    return true;
    }

  return false;
  }

std::shared_ptr<Widget::Ref> EventDispatcher::implDispatch(Widget &root, KeyEvent &event) {
  Widget* w = &root;
  while(w->astate.focus!=nullptr) {
    w = w->astate.focus;
    }
  if(w->wstate.focus) {
    auto ptr = w->selfReference();
    w->keyDownEvent(event);
    if(event.isAccepted())
      return ptr;
    }
  return nullptr;
  }

void EventDispatcher::implSetMouseOver(const std::shared_ptr<Widget::Ref> &wptr,MouseEvent& orig) {
  auto widget = wptr==nullptr ? nullptr : wptr->widget;
  if(auto old = mouseOver.lock()) {
    implExcMouseOver(widget,old->widget);

    auto p = orig.pos() - old->widget->mapToRoot(Point());
    MouseEvent e( p.x,
                  p.y,
                  Event::ButtonNone,
                  0,
                  0,
                  Event::MouseLeave );
    old->widget->mouseLeaveEvent(e);
    } else {
    implExcMouseOver(widget,nullptr);
    }

  mouseOver = wptr;
  if(wptr!=nullptr && wptr->widget!=nullptr) {
    auto p = orig.pos() - wptr->widget->mapToRoot(Point());
    MouseEvent e( p.x,
                  p.y,
                  Event::ButtonNone,
                  0,
                  0,
                  Event::MouseLeave );
    wptr->widget->mouseEnterEvent(e);
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

void EventDispatcher::handleModKey(const KeyEvent& e) {
  const bool v = e.type()==Event::KeyDown ? true : false;
  switch(e.key) {
    case Event::K_LControl:
      keyMod.ctrlL = v;
      break;
    case Event::K_RControl:
      keyMod.ctrlR = v;
      break;
    case Event::K_LShift:
      keyMod.shiftL = v;
      break;
    case Event::K_RShift:
      keyMod.shiftR = v;
      break;
    default:
      break;
    }
  }

std::shared_ptr<Widget::Ref> EventDispatcher::lock(std::weak_ptr<Widget::Ref>& w) {
  auto ptr = w.lock();
  if(ptr==nullptr)
    return nullptr;
  Widget* wx = ptr.get()->widget;
  while(wx->owner()!=nullptr) {
    wx = wx->owner();
    }
  if(dynamic_cast<Window*>(wx) || dynamic_cast<UiOverlay*>(wx) || wx==customRoot)
    return ptr;
  return nullptr;
  }

Event::Modifier EventDispatcher::mkModifier() const {
  uint8_t ret = 0;
  if(keyMod.ctrlL || keyMod.ctrlR)
    ret |= Event::Modifier::M_Ctrl;
  if(keyMod.altL || keyMod.altR)
    ret |= Event::Modifier::M_Alt;
  if(keyMod.shiftL || keyMod.shiftR)
    ret |= Event::Modifier::M_Shift;
#ifndef __OSX__
  if(keyMod.ctrlL || keyMod.ctrlR)
    ret |= Event::Modifier::M_Command;
#else
#error "TODO: implement apple-command key"
#endif
  return Event::Modifier(ret);
  }
