#pragma once

#include <cstdint>

namespace Tempest {

class PaintDevice;

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
    PaintEvent(PaintDevice & p,uint32_t w,uint32_t h):dev(p),outW(w),outH(h) {
      setType( Paint );
      }
    PaintEvent(PaintDevice & p,int32_t w,int32_t h):dev(p),outW(uint32_t(w)),outH(uint32_t(h)) {
      setType( Paint );
      }

    PaintEvent(PaintEvent& parent,int32_t dx,int32_t dy)
      :dev(parent.dev),dx(dx),dy(dy),outW(parent.outW),outH(parent.outH){
      setType( Paint );
      }

    PaintDevice& device()  { return dev;  }
    uint32_t     w() const { return outW; }
    uint32_t     h() const { return outH; }

  private:
    PaintDevice& dev;
    int32_t  dx=0;
    int32_t  dy=0;
    uint32_t outW=0;
    uint32_t outH=0;

    using Event::accept;
  };
}
