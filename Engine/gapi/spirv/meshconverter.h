#pragma once

#include <functional>
#include <unordered_set>

#include "libspirv/libspirv.h"

class MeshConverter {
  public:
    explicit MeshConverter(libspirv::MutableBytecode& code);
    libspirv::MutableBytecode& vertexPassthrough() { return vert; }

    void exec();
    void generateVs();

  private:
    struct VarItm {
      uint32_t type     = 0;
      };

    struct BaseTypes {
      uint32_t void_t  = -1;
      uint32_t bool_t  = -1;
      uint32_t uint32  = -1;
      uint32_t int32   = -1;
      uint32_t float32 = -1;

      uint32_t vec2    = -1;
      uint32_t vec3    = -1;
      uint32_t vec4    = -1;

      uint32_t uvec2   = -1;
      uint32_t uvec3   = -1;
      uint32_t uvec4   = -1;

      uint32_t ivec2   = -1;
      uint32_t ivec3   = -1;
      uint32_t ivec4   = -1;
      };

    void     avoidReservedFixup();
    void     removeMultiview(libspirv::MutableBytecode& code);

    void     injectLoop(libspirv::MutableBytecode::Iterator& fn,
                        uint32_t varI, uint32_t begin, uint32_t end, uint32_t inc, std::function<void(libspirv::MutableBytecode::Iterator& fn)> body);
    void     injectEngineBindings(const uint32_t idMainFunc);
    void     injectCountingPass(const uint32_t idMainFunc);
    void     replaceEntryPoint(const uint32_t idMainFunc, const uint32_t idEngineMain);

    libspirv::MutableBytecode& code;
    libspirv::MutableBytecode  vert;

    // meslet builtins
    uint32_t idPrimitiveCountNV   = -1;
    uint32_t idPrimind            = -1;
    uint32_t idPrimitiveIndicesNV = -1;
    uint32_t idLocalInvocationId  = -1;
    uint32_t idGlobalInvocationId = -1;
    uint32_t idWorkGroupID        = -1;
    uint32_t std450Import         = -1;

    std::unordered_map<uint32_t,VarItm> outVar;
  };

