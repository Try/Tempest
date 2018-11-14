#pragma once

#include <cstring>

namespace Tempest {

class Widget;

class Layout {
  public:
    Layout();
    virtual ~Layout()=default;

    size_t  count() const;
    Widget* at(size_t i);

  private:
    Widget* w=nullptr;

    void bind(Widget* w);

  friend class Widget;
  };

}
