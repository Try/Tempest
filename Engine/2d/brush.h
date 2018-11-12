#pragma once

#include <Tempest/Texture2d>

namespace Tempest {

class Brush {
  public:
    Brush(const Tempest::Texture2d& texture);

  private:
    const Tempest::Texture2d* tex=nullptr;

  friend class Painter;
  };

}
