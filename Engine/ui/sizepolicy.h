#pragma once

#include <Tempest/Size>

namespace Tempest {

enum SizePolicyType : uint8_t {
  Preferred=0,
  Fixed    =1,
  Expanding=2
  };

struct SizePolicy final {
  SizePolicy()=default;

  Size           minSize;
  Size           maxSize=maxWidgetSize();
  SizePolicyType typeH=Preferred;
  SizePolicyType typeV=Preferred;

  bool operator ==( const SizePolicy & other ) const;
  bool operator !=( const SizePolicy & other ) const;

  static Size maxWidgetSize();
  };


struct Margin final {
  Margin()=default;
  Margin( int v ):left(v), right(v), top(v), bottom(v) {}
  Margin( int l, int r, int t, int b ):left(l), right(r), top(t), bottom(b) {}

  int left=0, right=0, top=0, bottom=0;

  int xMargin() const { return left+right;  }
  int yMargin() const { return top +bottom; }

  bool operator == (const Margin& other) const {
    return left  ==other.left  &&
           right ==other.right &&
           top   ==other.top   &&
           bottom==other.bottom;
    }
  bool operator != (const Margin& other) const {
    return !(*this==other);
    }
  };
}
