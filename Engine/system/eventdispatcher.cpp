#include "eventdispatcher.h"

#include <Tempest/Application>
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
  MouseEvent e1( e.x,
                 e.y,
                 e.button,
                 mkModifier(),
                 e.delta,
                 e.mouseID,
                 Event::MouseDown );

  for(auto i:overlays) {
    if(!i->bind(wnd))
      continue;
    mouseUp = implDispatch(*i,e1);
    if(!mouseUp.expired())
      break;
    if(e1.type()==MouseEvent::MouseDown)
      mouseLast = mouseUp;
    }


  if(mouseUp.expired())
    mouseUp = implDispatch(wnd,e1);
  if(e1.type()==MouseEvent::MouseDown)
    mouseLast = mouseUp;

  if(auto w = mouseUp.lock()) {
    if(w->widget->focusPolicy() & ClickFocus) {
      w->widget->implSetFocus(true,Event::FocusReason::ClickReason);
      }
    }
  }

void EventDispatcher::dispatchMouseUp(Widget& /*wnd*/, MouseEvent &e) {
  auto ptr = mouseUp;
  mouseUp.reset();

  if(auto w = ptr.lock()) {
    auto p = e.pos() - w->widget->mapToRoot(Point());
    MouseEvent e1( p.x,
                   p.y,
                   e.button,
                   mkModifier(),
                   e.delta,
                   e.mouseID,
                   Event::MouseUp );
    w->widget->mouseUpEvent(e1);
    if(!e1.isAccepted())
      return;
    }
  }

void EventDispatcher::dispatchMouseMove(Widget &wnd, MouseEvent &e) {
  if(auto w = lock(mouseUp)) {
    auto p = e.pos() - w->widget->mapToRoot(Point());
    MouseEvent e0( p.x,
                   p.y,
                   Event::ButtonNone,
                   mkModifier(),
                   e.delta,
                   e.mouseID,
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
                   e.modifier,
                   e.delta,
                   e.mouseID,
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
                 mkModifier(),
                 e.delta,
                 e.mouseID,
                 Event::MouseMove  );
  for(auto i:overlays) {
    if(!i->bind(wnd))
      continue;
    auto wptr = implDispatch(*i,e1);
    if(wptr!=nullptr) {
      implSetMouseOver(wptr,e1);
      return;
      }
    }
  auto wptr = implDispatch(wnd,e1);
  implSetMouseOver(wptr,e1);
  }

void EventDispatcher::dispatchMouseWheel(Widget &wnd, MouseEvent &e) {
  MouseEvent e1( e.x,
                 e.y,
                 e.button,
                 mkModifier(),
                 e.delta,
                 e.mouseID,
                 Event::MouseWheel );
  for(auto i:overlays) {
    if(!i->bind(wnd))
      continue;
    implMouseWhell(*i,e1);
    if(e.isAccepted())
      return;
    }
  implMouseWhell(wnd,e1);
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

  for(auto i:overlays) {
    if(!i->bind(wnd))
      continue;
    if(implShortcut(*i,e1))
      return;
    k = implDispatch(*i,e1);
    if(!k.expired())
      return;
    }

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
    if(e.isAccepted())
      return;
    }
  wnd.closeEvent(e);
  }

void EventDispatcher::dispatchRender(Window& wnd) {
  if(wnd.w()>0 && wnd.h()>0)
    wnd.render();
  }

void EventDispatcher::dispatchOverlayRender(Window& wnd, PaintEvent& e) {
  for(size_t i=overlays.size(); i>0;) {
    --i;
    auto w = overlays[i];
    if(!w->bind(wnd))
      continue;
    w->astate.needToUpdate = false;
    w->dispatchPaintEvent(e);
    }
  }

void EventDispatcher::addOverlay(UiOverlay* ui) {
  overlays.insert(overlays.begin(),ui);
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

std::shared_ptr<Widget::Ref> EventDispatcher::implDispatch(Widget& w, MouseEvent &event) {
  if(!w.isVisible()) {
    event.ignore();
    return nullptr;
    }
  Point            pos=event.pos();
  Widget::Iterator it(&w);
  it.moveToEnd();

  for(;it.hasPrev();it.prev()) {
    Widget* i=it.get();
    if(i->rect().contains(pos)) {
      MouseEvent ex(event.x - i->x(),
                    event.y - i->y(),
                    event.button,
                    event.modifier,
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
    if(event.type()==Event::MouseDown) {
      auto     last     = mouseLast.lock();
      bool     dblClick = false;
      uint64_t time     = Application::tickCount();
      if(time-mouseLastTime<1000 && last!=nullptr && last->widget==it.owner) {
        dblClick = true;
        }
      mouseLastTime = time;
      if(dblClick)
        it.owner->mouseDoubleClickEvent(event); else
        it.owner->mouseDownEvent(event);
      } else {
      it.owner->mouseMoveEvent(event);
      }

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
  it.moveToEnd();
  for(;it.hasPrev();it.prev()) {
    Widget* i=it.get();
    if(i->rect().contains(pos)){
      MouseEvent ex(event.x - i->x(),
                    event.y - i->y(),
                    event.button,
                    event.modifier,
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

  if(!w.astate.focus && !w.wstate.focus)
    return false;

  std::lock_guard<std::recursive_mutex> guard(Widget::syncSCuts);
  for(auto& sc:w.sCuts) {
    if(!sc->isEnable())
      continue;
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
  if(w->wstate.focus || &root==w) {
    auto ptr = w->selfReference();
    w->keyDownEvent(event);
    if(event.isAccepted())
      return ptr;
    }
  return nullptr;
  }

void EventDispatcher::implSetMouseOver(const std::shared_ptr<Widget::Ref> &wptr,MouseEvent& orig) {
  auto    widget = wptr==nullptr ? nullptr : wptr->widget;
  Widget* oldW   = nullptr;
  if(auto old = mouseOver.lock())
    oldW = old->widget;

  if(widget==oldW)
    return;

  implExcMouseOver(widget,oldW);

  if(oldW!=nullptr) {
    auto p = orig.pos() - oldW->mapToRoot(Point());
    MouseEvent e( p.x,
                  p.y,
                  Event::ButtonNone,
                  orig.modifier,
                  0,
                  0,
                  Event::MouseLeave );
    oldW->mouseLeaveEvent(e);
    }

  mouseOver = wptr;
  if(widget!=nullptr) {
    auto p = orig.pos() - widget->mapToRoot(Point());
    MouseEvent e( p.x,
                  p.y,
                  Event::ButtonNone,
                  orig.modifier,
                  0,
                  0,
                  Event::MouseLeave );
    widget->mouseEnterEvent(e);
    }
  }

void EventDispatcher::implExcMouseOver(Widget* w, Widget* old) {
  auto* wx = old;
  while(wx!=nullptr) {
    wx->wstate.moveOver = false;
    wx = wx->owner();
    }

  wx = w;
  if(w!=nullptr) {
    auto root = w;
    while(root->owner()!=nullptr)
      root = root->owner();
    if(auto r = dynamic_cast<Window*>(root))
      r->implShowCursor(w->wstate.cursor);
    if(auto r = dynamic_cast<UiOverlay*>(root))
      r->implShowCursor(w->wstate.cursor);
    }
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
#ifdef __OSX__
    case Event::K_LCommand:
      keyMod.cmdL = v;
      break;
    case Event::K_RCommand:
      keyMod.cmdR = v;
      break;
#endif
    case Event::K_LShift:
      keyMod.shiftL = v;
      break;
    case Event::K_RShift:
      keyMod.shiftR = v;
      break;
    case Event::K_LAlt:
      keyMod.altL = v;
      break;
    case Event::K_RAlt:
      keyMod.altR = v;
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
  if(keyMod.cmdL || keyMod.cmdR)
    ret |= Event::Modifier::M_Command;
#endif
  return Event::Modifier(ret);
  }
