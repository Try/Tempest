#include "uniformslayout.h"

using namespace Tempest;

UniformsLayout &UniformsLayout::add(uint32_t layout,UniformsLayout::Class c,UniformsLayout::Stage s) {
  Binding e;
  e.layout=layout;
  e.cls   =c;
  e.stage =s;
  elt.push_back(e);
  impl=Detail::DSharedPtr<AbstractGraphicsApi::UniformsLay*>();
  return *this;
  }
