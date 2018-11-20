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
  Size           maxSize;
  SizePolicyType typeH=Preferred;
  SizePolicyType typeV=Preferred;

  bool operator ==( const SizePolicy & other ) const;
  bool operator !=( const SizePolicy & other ) const;

  static Size maxWidgetSize();
  };

}
