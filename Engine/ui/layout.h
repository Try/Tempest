#pragma once

#include <cstring>
#include "../utility/utility.h"

namespace Tempest {

class Widget;

class Layout {
  public:
    Layout();
    virtual ~Layout()=default;

    size_t  count() const;
    Widget* at(size_t i);
    size_t  find(Widget* w) const;
    Widget* takeWidget(size_t i);
    Widget* takeWidget(Widget* w);

    virtual void applyLayout(){}

  protected:
    Widget*       owner(){ return w; }
    const Widget* owner() const { return w; }

  private:
    Widget* w=nullptr;

    void bind(Widget* w);

  friend class Widget;
  };

class LinearLayout : public Layout {
  public:
    LinearLayout(Orienration ori):ori(ori){}

    void applyLayout() override { applyLayout(*owner(),ori); }
    void applyLayout(Widget& w,Orienration ori);

  private:
    Orienration ori;

    template<bool hor>
    void implApplyLayout(Widget& w);

    template<bool hor>
    void implApplyLayout(Widget& w,size_t count,size_t visCount,bool exp,int sum,int free,int expCount);
  };
}
