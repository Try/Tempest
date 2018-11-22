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

    size_t  widgetsCount() const { return wx.size(); }
    Widget& widget(size_t i) { return *wx[i]; }
    Widget& addWidget(Widget* w);
    Widget* takeWidget(Widget* w);

          Widget* owner()       { return ow; }
    const Widget* owner() const { return ow; }

    const Tempest::Rect& rect()     const { return wrect; }
    const Tempest::Size& sizeHint() const { return szHint; }

    void  setGeometry(const Rect& rect);
    void  setGeometry(int x,int y,int w,int h);
    void 	resize(const Size& size);
    void 	resize(int w,int h);

    int   w() const { return  wrect.w; }
    int   h() const { return  wrect.h; }

  protected:
    void setSizeHint(const Tempest::Size& s);
    void setSizeHint(int w,int h) { return setSizeHint(Size(w,h)); }

    void setSizePolicy(SizePolicyType h,SizePolicyType  v);
    void setSizePolicy(const SizePolicy& sp);

    virtual void paintEvent     (Tempest::PaintEvent& event);
    virtual void resizeEvent    (Tempest::SizeEvent&  event);

    virtual void mouseDownEvent (Tempest::MouseEvent& event);
    virtual void mouseUpEvent   (Tempest::MouseEvent& event);
    virtual void mouseMoveEvent (Tempest::MouseEvent& event);

  private:
    Widget*                 ow=nullptr;
    std::vector<Widget*>    wx;
    Tempest::Rect           wrect;
    Tempest::Size           szHint;
    Tempest::SizePolicy     szPolicy;

    struct Additive {
      uint16_t disable=0;
      };
    Additive                state;

    Layout*                 lay=reinterpret_cast<Layout*>(layBuf);
    char                    layBuf[sizeof(void*)*3]={};

    void                    freeLayout() noexcept;
    void                    dispatchPaintEvent(PaintEvent &e);

    void                    setOwner(Widget* w);

  friend class Layout;
  friend class Window;
  };

}
