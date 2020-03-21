#pragma once

#include <Tempest/Rect>
#include <Tempest/SizePolicy>
#include <Tempest/Event>
#include <Tempest/Style>
#include <Tempest/WidgetState>

#include <vector>
#include <memory>
#include <mutex>

namespace Tempest {

class Layout;
class Shortcut;

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

    Rect clientRect() const;

    void setEnabled(bool e);
    bool isEnabled() const;
    bool isEnabledTo(const Widget* ancestor) const;

    void setFocus(bool b);
    bool hasFocus() const { return wstate.focus; }

    void update();
    bool needToUpdate() const { return astate.needToUpdate; }

    bool isMouseOver()  const { return wstate.moveOver; }

    void               setStyle(const Style* stl);
    const Style&       style() const;

    const WidgetState& state() const { return wstate; }

  protected:
    void setSizeHint(const Tempest::Size& s);
    void setSizeHint(const Tempest::Size& s,const Margin& add);
    void setSizeHint(int w,int h) { return setSizeHint(Size(w,h)); }

    void setWidgetState(const WidgetState& st);

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
    virtual void closeEvent       (Tempest::CloseEvent& event);

  private:
    struct Iterator final {
      Iterator(Widget* owner);
      ~Iterator();

      void    onDelete();
      void    onDelete(size_t i,Widget* wx);
      bool    hasNext() const;
      void    next();
      Widget* get();
      Widget* getLast();

      size_t                id=0;
      Widget*               owner=nullptr;
      std::vector<Widget*>* nodes;
      std::vector<Widget*>  deleteLater;
      Widget*               getPtr=nullptr;
      };

    struct Ref {
      Ref(Widget* w):widget(w){}
      Widget* widget = nullptr;
      };

    struct Additive {
      Widget*  focus        = nullptr;
      uint16_t disable      = 0;
      bool     needToUpdate = false;
      };

    Widget*                 ow=nullptr;
    std::vector<Widget*>    wx;
    Tempest::Rect           wrect;
    Tempest::Size           szHint;
    Tempest::SizePolicy     szPolicy;
    FocusPolicy             fcPolicy=NoFocus;
    Tempest::Margin         marg;
    int                     spa=2;
    Iterator*               iterator=nullptr;

    Additive                astate;
    WidgetState             wstate;

    static std::recursive_mutex syncSCuts;
    std::vector<Shortcut*>  sCuts;

    std::shared_ptr<Ref>    selfRef;

    Layout*                 lay=reinterpret_cast<Layout*>(layBuf);
    char                    layBuf[sizeof(void*)*3]={};

    const Style*            stl = nullptr;

    void                    implRegisterSCut(Shortcut* s);
    void                    implUnregisterSCut(Shortcut* s);

    void                    freeLayout() noexcept;
    void                    implDisableSum(Widget *root,int diff) noexcept;
    Widget&                 implAddWidget(Widget* w);
    void                    implSetFocus(Widget* Additive::*add, bool WidgetState::*flag, bool value, const MouseEvent* parent);
    static void             implExcFocus(Event::Type type,Widget* prev, Widget* next, const MouseEvent& parent);
    static void             implClearFocus(Widget* w,Widget* Additive::*add, bool WidgetState::*flag);
    static Widget*          implTrieRoot(Widget* w);
    bool                    checkFocus() const { return wstate.focus || astate.focus; }
    void                    implAttachFocus();

    void                    dispatchPaintEvent(PaintEvent &e);

    auto                    selfReference() -> const std::shared_ptr<Ref>&;
    void                    setOwner(Widget* w);

  friend class EventDispatcher;
  friend class Layout;
  friend class Shortcut;
  friend class Window;
  };

template<class T>
T& Widget::addWidget(T* w){
  implAddWidget(w);
  return *w;
  }
}
