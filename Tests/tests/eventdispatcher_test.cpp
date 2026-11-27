#include <Tempest/Widget>
#include <Tempest/Button>

#include <Tempest/Window>
#include <Tempest/EventDispatcher>

#include <functional>

#include <gtest/gtest.h>
#include <gmock/gmock-matchers.h>

using namespace testing;
using namespace Tempest;

MouseEvent mkMEvent(Event::Type t, int x,int y){
  return MouseEvent(x,y,Event::ButtonLeft,Event::M_NoModifier,0,0,t);
  }

KeyEvent mkKEvent(Event::KeyType kt, Event::Type t){
  return KeyEvent(kt,t);
  }

struct TstButton:Button {
  int down=0;
  int up  =0;
  int move=0;

  void mouseDownEvent(Tempest::MouseEvent&) override {
    down++;
    }
  void mouseUpEvent(Tempest::MouseEvent&) override {
    up++;
    }
  void mouseMoveEvent(Tempest::MouseEvent&) override {
    move++;
    }

  void clear() {
    down=0;
    up  =0;
    move=0;
    }
  };

TEST(main,EventDispatcher_MouseEvent) {
  Widget wx;
  wx.resize(1000,50);
  wx.setLayout(Vertical);

  EventDispatcher dis(wx);

  TstButton& b0=wx.addWidget(new TstButton());
  TstButton& b1=wx.addWidget(new TstButton());

  auto evt0 = mkMEvent(Event::MouseDown,11,22);
  dis.dispatchMouseDown(wx,evt0);

  auto evt1 = mkMEvent(Event::MouseUp,11,22);
  dis.dispatchMouseUp(wx,evt1);

  EXPECT_EQ(b0.down,1);
  EXPECT_EQ(b0.up,  1);

  EXPECT_EQ(b1.down,0);
  EXPECT_EQ(b1.up,  0);

  b0.clear();
  b1.clear();

  auto evt3 = mkMEvent(Event::MouseDown,11,22);
  dis.dispatchMouseDown(wx,evt0);

  auto evt4 = mkMEvent(Event::MouseMove,999,22);
  dis.dispatchMouseMove(wx,evt4);
  EXPECT_EQ(b0.down,1);
  EXPECT_EQ(b0.up,  0);
  EXPECT_EQ(b0.move,1);

  auto evt5 = mkMEvent(Event::MouseUp,11,22);
  dis.dispatchMouseUp(wx,evt5);
  EXPECT_EQ(b0.down,1);
  EXPECT_EQ(b0.up,  1);
  EXPECT_EQ(b0.move,1);
  }

namespace {
struct PointerWidget : Widget {
  std::vector<MouseEvent> events;
  std::function<void(MouseEvent&)> onDown, onDrag, onUp;
  int doubleClicks = 0;

  void mouseDownEvent(MouseEvent& e) override {
    events.push_back(e);
    if(onDown)
      onDown(e);
    }
  void mouseDoubleClickEvent(MouseEvent& e) override {
    ++doubleClicks;
    mouseDownEvent(e);
    }
  void mouseDragEvent(MouseEvent& e) override {
    events.push_back(e);
    if(onDrag)
      onDrag(e);
    }
  void mouseMoveEvent(MouseEvent& e) override {
    events.push_back(e);
    }
  void mouseUpEvent(MouseEvent& e) override {
    events.push_back(e);
    if(onUp)
      onUp(e);
    }
  };

struct PointerDispatch : Test {
  Widget root;
  EventDispatcher dispatcher{root};
  PointerWidget& left  = root.addWidget(new PointerWidget());
  PointerWidget& right = root.addWidget(new PointerWidget());

  PointerDispatch() {
    root.resize(200,100);
    left.setGeometry(0,0,100,100);
    right.setGeometry(100,0,100,100);
    }

  void send(Event::Type type, int id, int x, Event::MouseButton button=Event::ButtonLeft) {
    MouseEvent e(x,20,button,Event::M_NoModifier,0,id,type);
    switch(type) {
      case Event::MouseDown: dispatcher.dispatchMouseDown(root,e); break;
      case Event::MouseUp:   dispatcher.dispatchMouseUp(root,e);   break;
      case Event::MouseMove: dispatcher.dispatchMouseMove(root,e); break;
      default: FAIL();
      }
    }
  };
}

TEST_F(PointerDispatch,SeparateWidgets) {
  for(int first:{0,1}) {
    left.events.clear();
    right.events.clear();
    send(Event::MouseDown,0,20);
    send(Event::MouseDown,1,120);
    send(Event::MouseMove,0,150);
    send(Event::MouseMove,1,50);
    send(Event::MouseUp,first,50);
    send(Event::MouseMove,1-first,150);
    send(Event::MouseUp,1-first,150);

    for(int id:{0,1}) {
      auto& events = id==0 ? left.events : right.events;
      ASSERT_EQ(events.size(),id==first ? 3u : 4u);
      EXPECT_EQ(events.front().type(),Event::MouseDown);
      EXPECT_EQ(events[1].type(),Event::MouseDrag);
      EXPECT_EQ(events[1].x,id==0 ? 150 : -50);
      EXPECT_EQ(events.back().type(),Event::MouseUp);
      for(auto& e:events)
        EXPECT_EQ(e.mouseID,id);
      }
    }
  }

TEST_F(PointerDispatch,SameWidget) {
  send(Event::MouseDown,0,20);
  send(Event::MouseDown,1,30);
  send(Event::MouseMove,0,150);
  send(Event::MouseMove,1,160);
  send(Event::MouseUp,1,160);
  send(Event::MouseUp,0,150);
  EXPECT_EQ(left.doubleClicks,0);
  ASSERT_EQ(left.events.size(),6u);
  EXPECT_EQ(left.events[2].mouseID,0);
  EXPECT_EQ(left.events[3].mouseID,1);
  EXPECT_EQ(left.events[4].mouseID,1);
  EXPECT_EQ(left.events[5].mouseID,0);
  EXPECT_EQ(left.events[4].type(),Event::MouseUp);
  EXPECT_EQ(left.events[5].type(),Event::MouseUp);
  }

TEST_F(PointerDispatch,MouseButtonsAndDoubleClick) {
  send(Event::MouseDown,0,20);
  send(Event::MouseDown,0,120,Event::ButtonRight);
  send(Event::MouseMove,0,180);
  send(Event::MouseUp,0,180);
  send(Event::MouseMove,0,20);
  send(Event::MouseUp,0,20,Event::ButtonRight);
  ASSERT_EQ(left.events.size(),3u);
  ASSERT_EQ(right.events.size(),3u);
  EXPECT_EQ(left.events[1].button,Event::ButtonLeft);
  EXPECT_EQ(right.events[1].button,Event::ButtonRight);
  send(Event::MouseDown,0,120,Event::ButtonRight);
  EXPECT_EQ(right.doubleClicks,1);
  send(Event::MouseUp,0,120,Event::ButtonRight);
  EXPECT_EQ(right.events.back().type(),Event::MouseUp);
  }

TEST_F(PointerDispatch,MouseButtonsReverseOrder) {
  send(Event::MouseDown,0,120,Event::ButtonRight);
  send(Event::MouseDown,0,20);
  send(Event::MouseMove,0,180);
  ASSERT_EQ(left.events.size(),2u);
  ASSERT_EQ(right.events.size(),1u);
  EXPECT_EQ(left.events.back().type(),Event::MouseDrag);
  EXPECT_EQ(left.events.back().button,Event::ButtonLeft);
  send(Event::MouseUp,0,180);
  send(Event::MouseUp,0,180,Event::ButtonRight);
  }

TEST_F(PointerDispatch,RepeatedDownReplacesCapture) {
  send(Event::MouseDown,0,20);
  send(Event::MouseDown,0,120);
  send(Event::MouseMove,0,20);
  send(Event::MouseUp,0,20);
  send(Event::MouseUp,0,20);
  ASSERT_EQ(left.events.size(),1u);
  ASSERT_EQ(right.events.size(),3u);
  EXPECT_EQ(right.events[1].type(),Event::MouseDrag);
  EXPECT_EQ(right.events[2].type(),Event::MouseUp);
  }

TEST_F(PointerDispatch,UncapturedPointer) {
  send(Event::MouseDown,0,20);
  send(Event::MouseUp,1,120);
  send(Event::MouseMove,1,120);
  send(Event::MouseMove,0,120);
  send(Event::MouseUp,0,120);
  ASSERT_EQ(left.events.size(),3u);
  EXPECT_EQ(left.events[1].type(),Event::MouseDrag);
  EXPECT_EQ(left.events[2].type(),Event::MouseUp);
  ASSERT_EQ(right.events.size(),1u);
  EXPECT_EQ(right.events[0].type(),Event::MouseMove);
  EXPECT_EQ(right.events[0].mouseID,1);
  }

TEST_F(PointerDispatch,NestedReleaseDuringDown) {
  left.onDown = [&](MouseEvent&) { send(Event::MouseUp,0,20); };
  send(Event::MouseDown,1,120);
  send(Event::MouseDown,0,20);
  send(Event::MouseMove,0,150);
  send(Event::MouseUp,1,150);
  ASSERT_EQ(left.events.size(),1u);
  ASSERT_EQ(right.events.size(),3u);
  EXPECT_EQ(right.events[1].type(),Event::MouseMove);
  EXPECT_EQ(right.events[1].mouseID,0);
  EXPECT_EQ(right.events[2].type(),Event::MouseUp);
  EXPECT_EQ(right.events[2].mouseID,1);
  }

TEST_F(PointerDispatch,NestedReleaseDuringDrag) {
  left.onDrag = [&](MouseEvent& e) {
    send(Event::MouseUp,0,20);
    e.ignore();
    };
  send(Event::MouseDown,0,20);
  send(Event::MouseMove,0,120);
  ASSERT_EQ(left.events.size(),3u);
  EXPECT_EQ(left.events.back().type(),Event::MouseUp);
  ASSERT_EQ(right.events.size(),1u);
  EXPECT_EQ(right.events[0].type(),Event::MouseMove);
  }

TEST_F(PointerDispatch,NestedDownDuringDrag) {
  left.onDrag = [&](MouseEvent& e) {
    send(Event::MouseDown,1,120);
    e.ignore();
    };
  send(Event::MouseDown,0,20);
  send(Event::MouseMove,0,120);
  ASSERT_EQ(left.events.size(),3u);
  EXPECT_EQ(left.events.back().type(),Event::MouseMove);
  EXPECT_EQ(left.events.back().mouseID,0);
  send(Event::MouseUp,0,120);
  send(Event::MouseMove,1,20);
  send(Event::MouseUp,1,20);
  ASSERT_EQ(right.events.size(),3u);
  EXPECT_EQ(right.events[1].type(),Event::MouseDrag);
  EXPECT_EQ(right.events[1].mouseID,1);
  EXPECT_EQ(right.events[2].type(),Event::MouseUp);
  }

TEST_F(PointerDispatch,NestedDownDuringRelease) {
  left.onUp = [&](MouseEvent&) { send(Event::MouseDown,0,120); };
  send(Event::MouseDown,0,20);
  send(Event::MouseUp,0,20);
  send(Event::MouseMove,0,20);
  send(Event::MouseUp,0,20);
  ASSERT_EQ(left.events.size(),2u);
  ASSERT_EQ(right.events.size(),3u);
  EXPECT_EQ(right.events[1].type(),Event::MouseDrag);
  EXPECT_EQ(right.events[2].type(),Event::MouseUp);
  }

TEST_F(PointerDispatch,DeletedCapture) {
  send(Event::MouseDown,0,20);
  send(Event::MouseDown,1,120);
  delete &left;
  send(Event::MouseMove,0,120);
  send(Event::MouseUp,0,120);
  send(Event::MouseMove,1,20);
  send(Event::MouseUp,1,20);
  ASSERT_EQ(right.events.size(),4u);
  EXPECT_EQ(right.events[1].type(),Event::MouseMove);
  EXPECT_EQ(right.events[1].mouseID,0);
  EXPECT_EQ(right.events[2].type(),Event::MouseDrag);
  EXPECT_EQ(right.events[2].mouseID,1);
  EXPECT_EQ(right.events[3].type(),Event::MouseUp);
  EXPECT_EQ(right.events[3].mouseID,1);
  }

TEST_F(PointerDispatch,DeletedDuringDown) {
  struct ClosingWidget : Widget {
    void mouseDownEvent(MouseEvent&) override { delete this; }
    };
  auto& closing = root.addWidget(new ClosingWidget());
  closing.setGeometry(0,0,100,100);
  send(Event::MouseDown,1,120);
  send(Event::MouseDown,0,20);
  send(Event::MouseMove,0,120);
  send(Event::MouseUp,1,120);
  ASSERT_EQ(right.events.size(),3u);
  EXPECT_EQ(right.events[1].type(),Event::MouseMove);
  EXPECT_EQ(right.events[1].mouseID,0);
  EXPECT_EQ(right.events[2].type(),Event::MouseUp);
  EXPECT_EQ(right.events[2].mouseID,1);
  }

namespace {
struct KeyCall {
  Event::Type     type;
  Event::KeyType  key      = Event::K_NoKey;
  uint32_t        code     = 0;
  Event::Modifier modifier = Event::M_NoModifier;
  bool operator==(const KeyCall& other) const {
    return type == other.type && key == other.key && code == other.code && modifier == other.modifier;
    }
  };

struct KeyWidget : Widget {
  std::vector<KeyCall> calls;

  KeyWidget() {
    setFocusPolicy(ClickFocus);
    }

  void keyDownEvent(KeyEvent& e) override { note(e); }
  void keyRepeatEvent(KeyEvent& e) override { note(e); }
  void keyUpEvent(KeyEvent& e) override { note(e); }
  void mouseDownEvent(MouseEvent& e) override { e.accept(); }
  void focusEvent(FocusEvent&) override { calls.push_back({Event::Focus}); }

  void note(KeyEvent& e) {
    calls.push_back({e.type(), e.key, e.code, e.modifier});
    e.accept();
    }
  };

struct KeyFocus : Test {
  Widget root;
  EventDispatcher dispatcher{root};
  KeyWidget& a = root.addWidget(new KeyWidget());
  KeyWidget& b = root.addWidget(new KeyWidget());

  KeyFocus() {
    root.resize(200, 100);
    a.setGeometry(0, 0, 100, 100);
    b.setGeometry(100, 0, 100, 100);
    }

  void down(Event::KeyType key, uint32_t scancode, uint32_t code = 0) {
    KeyEvent e(key, code, Event::M_NoModifier, Event::KeyDown);
    dispatcher.dispatchKeyDown(root, e, scancode);
    }
  void up(Event::KeyType key, uint32_t scancode, uint32_t code = 0) {
    KeyEvent e(key, code, Event::M_NoModifier, Event::KeyUp);
    dispatcher.dispatchKeyUp(root, e, scancode);
    }
  void focus(bool in) {
    FocusEvent e(in, Event::UnknownReason);
    dispatcher.dispatchFocus(root, e);
    }
  };
}

TEST_F(KeyFocus,HeldKeyReleasedBeforeFocusOut) {
  a.setFocus(true);
  down(Event::K_W, 17, 0x57);
  a.calls.clear();
  focus(false);
  EXPECT_EQ(a.calls, (std::vector<KeyCall>{{Event::KeyUp, Event::K_W, 0x57}, {Event::Focus}}));

  a.calls.clear();
  up(Event::K_W, 17, 0x57);
  EXPECT_TRUE(a.calls.empty());

  a.setFocus(true);
  down(Event::K_S, 31);
  a.calls.clear();
  focus(false);
  EXPECT_EQ(a.calls, (std::vector<KeyCall>{{Event::KeyUp, Event::K_S}}));

  focus(true);
  a.calls.clear();
  down(Event::K_S, 31);
  EXPECT_EQ(a.calls, (std::vector<KeyCall>{{Event::KeyDown, Event::K_S}}));
  }

TEST_F(KeyFocus,HeldModifierCleared) {
  struct Mod { Event::KeyType key; uint32_t scancode; };
  a.setFocus(true);
  for(Mod m : {Mod{Event::K_LAlt, 56}, Mod{Event::K_RAlt, 57}, Mod{Event::K_LShift, 42}, Mod{Event::K_RShift, 54},
             Mod{Event::K_LControl, 29}, Mod{Event::K_RControl, 97}}) {
    SCOPED_TRACE(int(m.key));
    down(m.key, m.scancode);
    down(Event::K_W, 17);
    Event::Modifier held = a.calls.back().modifier;
    EXPECT_NE(held, Event::M_NoModifier);

    a.calls.clear();
    focus(false);
    ASSERT_EQ(a.calls.size(), 3u);
    EXPECT_EQ(a.calls[0].modifier, held);
    EXPECT_EQ(a.calls[1].modifier, held);

    focus(true);
    a.calls.clear();
    down(Event::K_A, 30);
    up(Event::K_A, 30);
    EXPECT_EQ(a.calls, (std::vector<KeyCall>{{Event::KeyDown, Event::K_A}, {Event::KeyUp, Event::K_A}}));
    }
  }

TEST_F(KeyFocus,HolderDeletesItself) {
  struct ClosingWidget : KeyWidget {
    int& ups;
    ClosingWidget(int& ups):ups(ups) {}
    void keyUpEvent(KeyEvent&) override {
      ++ups;
      delete this;
      }
    };
  int ups = 0;
  auto& closing = root.addWidget(new ClosingWidget(ups));
  closing.setFocus(true);
  down(Event::K_W, 17);
  down(Event::K_S, 31);
  focus(false);
  EXPECT_EQ(ups, 1);
  }

TEST_F(KeyFocus,HolderDeletesWindow) {
  struct ClosingWidget : KeyWidget {
    void keyUpEvent(KeyEvent&) override { delete owner(); }
    };
  auto* window = new Widget();
  window->addWidget(new ClosingWidget()).setFocus(true);
  EventDispatcher windowDispatcher{*window};
  KeyEvent   press(Event::K_W, 0, Event::M_NoModifier, Event::KeyDown);
  FocusEvent out(false, Event::UnknownReason);
  windowDispatcher.dispatchKeyDown(*window, press, 17);
  windowDispatcher.dispatchFocus(*window, out);
  }

TEST_F(KeyFocus,DestroyedWindowReleasesNothing) {
  a.setFocus(true);
  down(Event::K_W, 17);
  dispatcher.dispatchDestroyWindow(nullptr);
  a.calls.clear();
  focus(false);
  for(auto& i : a.calls)
    EXPECT_NE(i.type, Event::KeyUp);
  }

TEST_F(KeyFocus,OnlyFocusOutReleases) {
  a.setFocus(true);
  down(Event::K_W, 17);
  a.calls.clear();

  MouseEvent click(150, 20, Event::ButtonLeft, Event::M_NoModifier, 0, 0, Event::MouseDown);
  dispatcher.dispatchMouseDown(root, click);
  EXPECT_TRUE(b.hasFocus());
  a.setFocus(true);
  for(auto& i : a.calls)
    EXPECT_NE(i.type, Event::KeyUp);

  a.calls.clear();
  down(Event::K_W, 17);
  EXPECT_EQ(a.calls, (std::vector<KeyCall>{{Event::KeyRepeat, Event::K_W}}));
  }

TEST_F(KeyFocus,FocusOutInsideKeyDown) {
  struct LeavingWidget : KeyWidget {
    KeyFocus& test;
    LeavingWidget(KeyFocus& test):test(test) {}
    void keyDownEvent(KeyEvent& e) override { note(e); test.focus(false); }
    };
  auto& leaving = root.addWidget(new LeavingWidget(*this));
  leaving.setFocus(true);
  leaving.calls.clear();
  down(Event::K_W, 17);
  up(Event::K_W, 17);
  EXPECT_EQ(leaving.calls, (std::vector<KeyCall>{{Event::KeyDown, Event::K_W}, {Event::Focus}}));
  }
