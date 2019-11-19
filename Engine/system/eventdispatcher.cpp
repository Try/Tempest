#include "eventdispatcher.h"

using namespace Tempest;

EventDispatcher::EventDispatcher() {
  }

void EventDispatcher::dispatchMouseDown(SystemApi::WindowCallback &wc, MouseEvent &e) {
  wc.onMouse(e);
  }

void EventDispatcher::dispatchMouseUp(SystemApi::WindowCallback &wc, MouseEvent &e) {
  wc.onMouse(e);
  }

void EventDispatcher::dispatchMouseMove(SystemApi::WindowCallback &wc, MouseEvent &e) {
  MouseEvent e0( e.x,
                 e.y,
                 Event::ButtonNone,
                 0,
                 0,
                 Event::MouseDrag  );
  wc.onMouse(e0);
  if(e0.isAccepted())
    return;

  MouseEvent e1( e0.x,
                 e0.y,
                 Event::ButtonNone,
                 0,
                 0,
                 Event::MouseMove  );
  wc.onMouse(e1);
  }

void EventDispatcher::dispatchMouseWheel(SystemApi::WindowCallback &wc, MouseEvent &e){
  wc.onMouse(e);
  }

void EventDispatcher::dispatchKeyDown(SystemApi::WindowCallback &wc, KeyEvent &e) {
  wc.onKey(e);
  }

void EventDispatcher::dispatchKeyUp(SystemApi::WindowCallback &wc, KeyEvent &e) {
  wc.onKey(e);
  }

