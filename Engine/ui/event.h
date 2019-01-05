#pragma once

#include <Tempest/Size>
#include <cstdint>

namespace Tempest {

class PaintDevice;
class TextureAtlas;
class Painter;

class Event {
  public:
    Event()=default;
    virtual ~Event(){}

    void accept(){ accepted = true;  }
    void ignore(){ accepted = false; }

    bool isAccepted() const {
      return accepted;
      }

    enum Type {
      NoEvent = 0,
      MouseDown,
      MouseUp,
      MouseMove,
      MouseDrag,
      MouseWheel,
      MouseEnter,
      MouseLeave,
      KeyDown,
      KeyUp,
      Focus,
      Resize,
      Shortcut,
      Paint,
      Close,
      Gesture,

      Custom = 512
      };

    enum MouseButton {
      ButtonNone = 0,
      ButtonLeft,
      ButtonRight,
      ButtonMid,
      ButtonBack,
      ButtonForward
      };

    enum KeyType {
      K_NoKey = 0,
      K_ESCAPE,

      K_Control,
      K_Command, // APPLE command key

      K_Left,
      K_Up,
      K_Right,
      K_Down,

      K_Back,
      K_Tab,
      K_Delete,
      K_Insert,
      K_Return,
      K_Home,
      K_End,
      K_Pause,
      K_Shift,

      K_F1,
      K_F2,
      K_F3,
      K_F4,
      K_F5,
      K_F6,
      K_F7,
      K_F8,
      K_F9,
      K_F10,
      K_F11,
      K_F12,
      K_F13,
      K_F14,
      K_F15,
      K_F16,
      K_F17,
      K_F18,
      K_F19,
      K_F20,
      K_F21,
      K_F22,
      K_F23,
      K_F24,

      K_A,
      K_B,
      K_C,
      K_D,
      K_E,
      K_F,
      K_G,
      K_H,
      K_I,
      K_J,
      K_K,
      K_L,
      K_M,
      K_N,
      K_O,
      K_P,
      K_Q,
      K_R,
      K_S,
      K_T,
      K_U,
      K_V,
      K_W,
      K_X,
      K_Y,
      K_Z,

      K_0,
      K_1,
      K_2,
      K_3,
      K_4,
      K_5,
      K_6,
      K_7,
      K_8,
      K_9,

      K_Last
      };

    enum FocusReason {
      TabReason,
      ClickReason,
      WheelReason,
      UnknownReason
      };

    Type type () const{ return etype; }

  protected:
    void setType( Type t ){ etype = t; }

  private:
    bool accepted=true;
    Type etype   =NoEvent;
  };

class PaintEvent: public Event {
  public:
    PaintEvent(PaintDevice & p,TextureAtlas& ta,uint32_t w,uint32_t h)
      : PaintEvent(p,ta,int(w),int(h)) {
      }
    PaintEvent(PaintDevice & p,TextureAtlas& ta,int32_t w,int32_t h)
      : dev(p),ta(ta),outW(uint32_t(w)),outH(uint32_t(h)),
        vp(0,0,w,h) {
      setType( Paint );
      }

    PaintEvent(PaintEvent& parent,int32_t dx,int32_t dy,int32_t x,int32_t y,int32_t w,int32_t h)
      : dev(parent.dev),ta(parent.ta),outW(parent.outW),outH(parent.outH),
        dp(parent.dp.x+dx,parent.dp.y+dy),vp(x,y,w,h){
      setType( Paint );
      }

    PaintDevice& device()  { return dev;  }
    uint32_t     w() const { return outW; }
    uint32_t     h() const { return outH; }

    const Point& orign()    const { return dp; }
    const Rect&  viewPort() const { return vp; }

  private:
    PaintDevice&  dev;
    TextureAtlas& ta;
    uint32_t      outW=0;
    uint32_t      outH=0;

    Point         dp;
    Rect          vp;

    using Event::accept;

  friend class Painter;
  };

/*!
 * \brief The SizeEvent class contains event parameters for resize events.
 */
class SizeEvent : public Event {
  public:
    SizeEvent(int w,int h): w(uint32_t(std::max(w,0))), h(uint32_t(std::max(h,0))) {
      setType( Resize );
      }
    SizeEvent(uint32_t w,uint32_t h): w(w), h(h) {
      setType( Resize );
      }

    const uint32_t w, h;

    Tempest::Size size() const { return Size(int(w),int(h)); }
  };

/*!
 * \brief The MouseEvent class contains event parameters for mouse and touch events.
 */
class MouseEvent : public Event {
  public:
    MouseEvent( int mx = -1, int my = -1,
                MouseButton b = ButtonNone,
                int mdelta = 0,
                int mouseID = 0,
                Type t = MouseMove )
      :x(mx), y(my), delta(mdelta), button(b), mouseID(mouseID){
      setType( t );
      }

    const int x, y, delta;
    const MouseButton button;

    const int mouseID;

    Point pos() const { return Point(x,y); }
  };
}
