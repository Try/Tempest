#pragma once

#include <Tempest/Rect>
#include <Tempest/SizePolicy>
#include <Tempest/Event>

#include <memory>

namespace Tempest {

class Layout;

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

    void update();
    bool needToUpdate() const { return state.needToUpdate; }

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

    virtual void dispatchMoveEvent(Tempest::MouseEvent& event);

  private:
    Widget*                 ow=nullptr;
    std::vector<Widget*>    wx;
    Tempest::Rect           wrect;
    Tempest::Size           szHint;
    Tempest::SizePolicy     szPolicy;
    Tempest::Margin         marg;
    int                     spa=2;

    struct Additive {
      Widget*  mouseFocus=nullptr;

      uint16_t disable     =0;
      bool     needToUpdate=false;
      };
    Additive                state;

    struct State {
      bool                  enabled=true;
      };
    State                   wstate;

    Layout*                 lay=reinterpret_cast<Layout*>(layBuf);
    char                    layBuf[sizeof(void*)*3]={};

    void                    freeLayout() noexcept;
    void                    implDisableSum(Widget *root,int diff) noexcept;
    Widget&                 implAddWidget(Widget* w);

    void                    dispatchPaintEvent(PaintEvent &e);

    void                    dispatchMouseDown (Tempest::MouseEvent &e);
    void                    dispatchMouseUp   (Tempest::MouseEvent &e);
    void                    dispatchMouseMove (Tempest::MouseEvent &e);
    void                    dispatchMouseDrag (Tempest::MouseEvent &e);

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
