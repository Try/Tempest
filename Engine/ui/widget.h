#pragma once

#include <Tempest/Rect>
#include <memory>

namespace Tempest {

class Layout;

class Widget {
  public:
    Widget();
    Widget(const Widget&)=delete;
    virtual ~Widget();

    void setLayout(Layout* lay);
    const Layout& layout() const { return *lay; }

    size_t  widgetsCount() const { return wx.size(); }
    Widget* child(size_t i) { return wx[i]; }

    const Tempest::Rect& rect() const { return wrect; }

  private:
    std::vector<Widget*>    wx;
    Tempest::Rect           wrect;

    std::unique_ptr<Layout> lay;

  friend class Layout;
  };

}
