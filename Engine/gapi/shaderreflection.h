#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <vector>

#include "thirdparty/spirv_cross/spirv_cross.hpp"

namespace Tempest {
namespace Detail {

class ShaderReflection final {
  public:
    enum Class : uint8_t {
      Ubo    =0,
      Texture=1,
      SsboR  =2,
      SsboRW =3,
      ImgR   =4,
      ImgRW  =5,
      Push   =6,
      };

    enum Stage : uint8_t {
      Vertex  =1<<0,
      Control =1<<1,
      Evaluate=1<<2,
      Geometry=1<<3,
      Fragment=1<<4,
      Compute =1<<5,
      };

    struct Binding {
      uint32_t layout=0;
      Class    cls   =Ubo;
      Stage    stage =Fragment;
      uint64_t size  =0;

      spirv_cross::ID spvId;
      uint32_t        mslBinding  = uint32_t(-1);
      uint32_t        mslBinding2 = uint32_t(-1);
      };

    struct PushBlock {
      Stage    stage = Fragment;
      uint64_t size  = 0;
      };

    static void getVertexDecl(std::vector<Decl::ComponentType>& data, spirv_cross::Compiler& comp);
    static void getBindings(std::vector<Binding>& b, spirv_cross::Compiler& comp);

    static void merge(std::vector<Binding>& ret,
                      PushBlock& pb,
                      const std::vector<Binding>& comp);
    static void merge(std::vector<Binding>& ret,
                      PushBlock& pb,
                      const std::vector<Binding>* sh[],
                      size_t count);

  private:
    static void finalize(std::vector<Binding>& p);
  };

}
}

