#pragma once

#include <Tempest/AbstractGraphicsApi>
#include "../utility/dptr.h"
#include "../utility/spinlock.h"

#include <cstdint>
#include <vector>
#include <memory>

namespace Tempest {

class Device;

class UniformsLayout final {
  public:
    enum Class : uint8_t {
      Ubo    =0,
      UboDyn =1,
      Texture=2
      };
    enum Stage : uint8_t {
      Vertex  =1<<0,
      Fragment=1<<1,
      };
    struct Binding {
      uint32_t layout=0;
      Class    cls   =Ubo;
      Stage    stage=Fragment;
      };

    UniformsLayout()=default;

    UniformsLayout& add(uint32_t layout,Class c,Stage s);
    const Binding& operator[](size_t i) const { return elt[i]; }
    size_t size() const { return elt.size(); }

  private:
    std::vector<Binding> elt;
    mutable Detail::SpinLock                                      sync;
    mutable Detail::DSharedPtr<AbstractGraphicsApi::UniformsLay*> impl;

  friend class Device;
  };

}
