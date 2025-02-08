#pragma once

#include <Tempest/AbstractGraphicsApi>
#include <vector>

#include "thirdparty/spirv_cross/spirv_cross.hpp"

namespace libspirv {
class Bytecode;
};

namespace Tempest {
namespace Detail {

class ShaderReflection final {
  public:
    enum Class : uint8_t {
      Ubo     = 0,
      Texture = 1,
      Image   = 2,
      Sampler = 3,
      SsboR   = 4,
      SsboRW  = 5,
      ImgR    = 6,
      ImgRW   = 7,
      Tlas    = 8,
      Push    = 9,
      Count,
      };

    enum Stage : uint8_t {
      None    =0,
      Vertex  =1<<0,
      Control =1<<1,
      Evaluate=1<<2,
      Geometry=1<<3,
      Fragment=1<<4,
      Compute =1<<5,
      Task    =1<<6,
      Mesh    =1<<7,
      };

    struct Binding {
      uint32_t        layout       = 0;
      Class           cls          = Ubo;
      Stage           stage        = None;
      bool            runtimeSized = false;
      bool            is3DImage    = false;
      uint32_t        arraySize    = 0;
      uint64_t        byteSize     = 0;
      uint64_t        varByteSize  = 0;

      bool            hasSampler() const { return cls==Texture || cls==Sampler; }

      spirv_cross::ID spvId;
      uint32_t        mslBinding  = uint32_t(-1);
      uint32_t        mslBinding2 = uint32_t(-1);
      uint64_t        mslSize     = 0;
      };

    struct LayoutDesc {
      ShaderReflection::Class bindings[MaxBindings] = {};
      ShaderReflection::Stage stage   [MaxBindings] = {};
      uint32_t                count   [MaxBindings] = {};
      uint32_t                runtime = 0;
      uint32_t                array   = 0;
      uint32_t                active  = 0;

      bool operator == (const LayoutDesc& other) const;
      bool operator != (const LayoutDesc& other) const;

      bool isUpdateAfterBind() const;
      };

    struct SyncDesc {
      uint32_t read  = 0;
      uint32_t write = 0;
      };

    struct PushBlock {
      Stage    stage = None;
      uint64_t size  = 0;
      };

    static void   getVertexDecl(std::vector<Decl::ComponentType>& data, spirv_cross::Compiler& comp);
    static void   getBindings(std::vector<Binding>& b, spirv_cross::Compiler& comp);
    static Stage  getExecutionModel(spirv_cross::Compiler& comp);
    static Stage  getExecutionModel(libspirv::Bytecode& comp);
    static Stage  getExecutionModel(spv::ExecutionModel m);
    static size_t mslSizeOf(const spirv_cross::ID& spvId, spirv_cross::Compiler& comp);

    static void   merge(std::vector<Binding>& ret,
                        PushBlock& pb,
                        const std::vector<Binding>* sh[],
                        size_t count);

    static size_t  sizeofBuffer(const Binding& bind, size_t arraylen);

  private:
    static void finalize(std::vector<Binding>& p);
    static size_t mslSizeOf(const spirv_cross::SPIRType& type, spirv_cross::Compiler& comp);
  };

}
}

