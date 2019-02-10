#pragma once

#include <Tempest/Rect>
#include <Tempest/SizePolicy>
#include <Tempest/Event>

#include <memory>

namespace Tempest {

class Layout;

enum FocusPolicy : uint8_t {
  NoFocus     = 0,
  TabFocus    = 1,
  ClickFocus  = 2,
  StrongFocus = TabFocus|ClickFocus,
  WheelFocus  = 4|StrongFocus
  };

class Widget {
  public:
    Widget();
    Widget(const Widget&)=delete;
    virtual ~Widget();

    void setLayout(Orienration ori) noexcept;
    void setLayout(Layout* lay);
    const Layout& layout() const { return *lay; }
    void  applyLayout();

    size_t  widgetsCount() const { return wx.size(); }
    Widget& widget(size_t i) { return *wx[i]; }
    void    removeAllWidgets();
    template<class T>
    T&      addWidget(T* w);
    Widget* takeWidget(Widget* w);

          Widget* owner()       { return ow; }
    const Widget* owner() const { return ow; }

    Point   mapToRoot( const Point & p ) const;

    Point                pos()      const { return Point(wrect.x,wrect.y);}
    const Tempest::Rect& rect()     const { return wrect; }
    Tempest::Size        size()     const { return Size(wrect.w,wrect.h); }
    const Tempest::Size& sizeHint() const { return szHint; }

    void  setPosition(int x,int y);
    void  setPosition(const Point& pos);
    void  setGeometry(const Rect& rect);
    void  setGeometry(int x,int y,int w,int h);
    void 	resize(const Size& size);
    void 	resize(int w,int h);

    int   x() const { return  wrect.x; }
    int   y() const { return  wrect.y; }
    int   w() const { return  wrect.w; }
    int   h() const { return  wrect.h; }

    void setSizePolicy(SizePolicyType hv);
    void setSizePolicy(SizePolicyType h,SizePolicyType  v);
    void setSizePolicy(const SizePolicy& sp);
    const SizePolicy& sizePolicy() const { return szPolicy; }

    FocusPolicy focusPolicy() const { return fcPolicy; }
    void setFocusPolicy( FocusPolicy f );

    void setMargins(const Margin& m);
    const Margin& margins() const { return marg; }

    const Size& minSize() const { return szPolicy.minSize; }
    const Size& maxSize() const { return szPolicy.maxSize; }

    void setMaximumSize( const Size & s );
    void setMinimumSize( const Size & s );

    void setMaximumSize( int w, int h );
    void setMinimumSize( int w, int h );

    void setSpacing( int s );
    int  spacing() const { return spa; }

    Rect clentRet() const;

    void setEnabled(bool e);
    bool isEnabled() const;
    bool isEnabledTo(const Widget* ancestor) const;

    void setFocus(bool b);
    bool hasFocus() const { return wstate.focus; }

    void update();
    bool needToUpdate() const { return state.needToUpdate; }

    bool isMouseOver()  const { return wstate.moveFocus; }

  protected:
    void setSizeHint(const Tempest::Size& s);
    void setSizeHint(const Tempest::Size& s,const Margin& add);
    void setSizeHint(int w,int h) { return setSizeHint(Size(w,h)); }

    virtual void paintEvent       (Tempest::PaintEvent& event);
    virtual void resizeEvent      (Tempest::SizeEvent&  event);

    virtual void mouseDownEvent   (Tempest::MouseEvent& event);
    virtual void mouseUpEvent     (Tempest::MouseEvent& event);
    virtual void mouseMoveEvent   (Tempest::MouseEvent& event);
    virtual void mouseDragEvent   (Tempest::MouseEvent& event);
    virtual void mouseWheelEvent  (Tempest::MouseEvent& event);

    virtual void keyDownEvent     (Tempest::KeyEvent&   event);
    virtual void keyUpEvent       (Tempest::KeyEvent&   event);

    virtual void mouseEnterEvent  (Tempest::MouseEvent& event);
    virtual void mouseLeaveEvent  (Tempest::MouseEvent& event);

    virtual void focusEvent       (Tempest::FocusEvent& event);

    virtual void dispatchMoveEvent(Tempest::MouseEvent& event);
    virtual void dispatchKeyEvent (Tempest::KeyEvent&   event);

  private:
    struct Iterator;

    Widget*                 ow=nullptr;
    std::vector<Widget*>    wx;
    Tempest::Rect           wrect;
    Tempest::Size           szHint;
    Tempest::SizePolicy     szPolicy;
    FocusPolicy             fcPolicy=NoFocus;
    Tempest::Margin         marg;
    int                     spa=2;
    Iterator*               iterator=nullptr;

    struct Additive {
      Widget*  mouseFocus=nullptr;
      Widget*  moveFocus =nullptr;
      Widget*  focus     =nullptr;

      uint16_t disable     =0;
      bool     needToUpdate=false;
      };
    Additive                state;

    struct State {
      bool                  disabled =false;
      bool                  focus    =false;
      bool                  moveFocus=false;
      bool                  mousePressed=false;
      };
    State                   wstate;

    Layout*                 lay=reinterpret_cast<Layout*>(layBuf);
    char                    layBuf[sizeof(void*)*3]={};

    void                    freeLayout() noexcept;
    void                    implDisableSum(Widget *root,int diff) noexcept;
    Widget&                 implAddWidget(Widget* w);
    void                    implSetFocus(Widget* Additive::*add,bool State::* flag,bool value,const MouseEvent* parent);
    static void             implExcFocus(Event::Type type,Widget* prev, Widget* next, const MouseEvent& parent);
    static void             implClearFocus(Widget* w,Widget* Additive::*add, bool State::*flag);
    static Widget*          implTrieRoot(Widget* w);
    bool                    checkFocus() const { return wstate.focus || state.focus; }
    void                    implAttachFocus();

    void                    dispatchPaintEvent(PaintEvent &e);

    void                    dispatchMouseDown (Tempest::MouseEvent &e);
    void                    dispatchMouseUp   (Tempest::MouseEvent &e);
    void                    dispatchMouseMove (Tempest::MouseEvent &e);
    void                    dispatchMouseDrag (Tempest::MouseEvent &e);
    void                    dispatchMouseWhell(Tempest::MouseEvent &e);
    void                    dispatchKeyDown   (Tempest::KeyEvent   &e);
    void                    dispatchKeyUp     (Tempest::KeyEvent   &e);

    void                    setOwner(Widget* w);

  friend class Layout;
  friend class Window;
  };

template<class T>
T& Widget::addWidget(T* w){
  implAddWidget(w);
  return *w;
  }
}
