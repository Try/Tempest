#include "layout.h"

#include <Tempest/Widget>

using namespace Tempest;

Layout::Layout() {
  }

size_t Layout::count() const {
  return w ? w->wx.size() : 0;
  }

Widget *Layout::at(size_t i) {
  return w->wx[i];
  }

void Layout::bind(Widget *wx) {
  w = wx;
  }
