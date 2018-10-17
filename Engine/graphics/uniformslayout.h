#pragma once

#include <cstdint>
#include <vector>

namespace Tempest {

class UniformsLayout {
  public:
    enum Class : uint8_t {
      Ubo    =0,
      Texture=1
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
  };

}
