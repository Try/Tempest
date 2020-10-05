#pragma once

#include <Tempest/UniformsLayout>
#include <vector>

#include "thirdparty/spirv_cross/spirv_cross.hpp"

namespace Tempest {
namespace Detail {

class ShaderReflection final {
  public:
    using Binding   = UniformsLayout::Binding;
    using PushBlock = UniformsLayout::PushBlock;

    static void getBindings(std::vector<Binding>& b, const uint32_t* sprv, uint32_t size);
    static void getBindings(std::vector<Binding>& b, spirv_cross::Compiler& comp);

    static void merge(std::vector<Binding>& ret,
                      PushBlock& pb,
                      const std::vector<Binding>& comp);
    static void merge(std::vector<Binding>& ret,
                      PushBlock& pb,
                      const std::vector<Binding>& vs,
                      const std::vector<Binding>& fs);
  };

}
}

