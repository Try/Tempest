#include "widget.h"

#include <Tempest/Layout>

using namespace Tempest;

Widget::Widget() {
  }

Widget::~Widget() {
  for(auto& w:wx)
    delete w;
  }

void Widget::setLayout(Layout *l) {
  lay.reset(l);
  l->bind(this);
  }
