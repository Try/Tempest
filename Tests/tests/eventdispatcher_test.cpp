#include <Tempest/Widget>
#include <Tempest/Button>

#include <Tempest/Window>
#include <Tempest/EventDispatcher>

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
