#pragma once

#include <Tempest/Widget>
#include <Tempest/ScrollBar>

namespace Tempest {

class ListView;

class ScrollWidget : public Tempest::Widget {
  public:
    ScrollWidget();
    ScrollWidget(Tempest::Orientation ori);
    ~ScrollWidget();

    enum ScrollViewMode : uint8_t {
      AlwaysOff,
      AsNeed,
      AlwaysOn
      };

    Widget& centralWidget();

    void    setLayout(Tempest::Orientation ori);

    void    hideScrollBars();
    void    setScrollBarsVisible( bool h, bool v );
    void    setVscrollViewMode(ScrollViewMode mode);
    void    setHscrollViewMode(ScrollViewMode mode);

    void    scrollAfterEndH( bool s );
    bool    hasScrollAfterEndH() const;

    void    scrollBeforeBeginH( bool s );
    bool    hasScrollBeforeBeginH() const;

    void    scrollAfterEndV( bool s );
    bool    hasScrollAfterEndV() const;

    void    scrollBeforeBeginV( bool s );
    bool    hasScrollBeforeBeginV() const;

    void    scrollH( int v );
    void    scrollV( int v );

    int     scrollH() const;
    int     scrollV() const;

  protected:
    void    mouseWheelEvent(Tempest::MouseEvent &e);
    void    mouseMoveEvent(Tempest::MouseEvent &e);

    virtual Size contentAreaSize();

  private:
    struct BoxLayout;
    struct ProxyLayout;
    struct Central:Widget {
      using Widget::setSizeHint;
      };

    bool    updateScrolls(Orientation orient, bool noRetry);
    void    emplace(Widget& cen, Widget* scH, Widget* scV, const Rect &place);

    Widget* findFirst();
    Widget* findLast();

    void    complexLayout();
    void    wrapContent();

    Central        cen;
    Widget         helper;
    ScrollBar      sbH;
    ScrollBar      sbV;
    ScrollViewMode vert = AsNeed;
    ScrollViewMode hor  = AsNeed;
    BoxLayout*     cenLay = nullptr;

    bool           scAfterEndH    = false;
    bool           scBeforeBeginH = false;

    bool           scAfterEndV    = true;
    bool           scBeforeBeginV = false;

    bool           layoutBusy = false;

    using Tempest::Widget::layout;
  };

}
