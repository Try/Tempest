#include "sizepolicy.h"

using namespace Tempest;

bool SizePolicy::operator ==(const SizePolicy &other) const {
  return maxSize==other.maxSize &&
         minSize==other.minSize &&
         typeH==other.typeH &&
         typeV==other.typeV;
  }

bool SizePolicy::operator !=(const SizePolicy &other) const {
  return !(*this==other);
  }

Size SizePolicy::maxWidgetSize()  {
  static Size s = Size( 1<<16, 1<<16 );
  return s;
  }
