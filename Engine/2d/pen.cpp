#include "pen.h"

using namespace Tempest;

Pen::Pen(const Color &cl, PaintDevice::Blend blend, float w)
  :color(cl),blend(blend),penW(w) {
  }
