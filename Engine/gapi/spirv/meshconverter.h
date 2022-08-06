#pragma once

#include <functional>
#include <unordered_set>
#include <unordered_map>

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
      uint32_t location = -1;
      };

    void     avoidReservedFixup();
    void     removeMultiview(libspirv::MutableBytecode& code);
    void     removeCullClip(libspirv::MutableBytecode& code);
    void     removeFromPerVertex(libspirv::MutableBytecode& code, const std::unordered_set<uint32_t>& fields);

    void     injectLoop(libspirv::MutableBytecode::Iterator& fn,
                        uint32_t varI, uint32_t begin, uint32_t end, uint32_t inc, std::function<void(libspirv::MutableBytecode::Iterator& fn)> body);
    void     injectEngineBindings(const uint32_t idMainFunc);
    void     injectCountingPass(const uint32_t idMainFunc);
    void     replaceEntryPoint(const uint32_t idMainFunc, const uint32_t idEngineMain);

    void     vsTypeRemaps(libspirv::MutableBytecode::Iterator& fn, std::unordered_map<uint32_t, uint32_t>& typeRemaps,
                          const libspirv::Bytecode::AccessChain* ids, uint32_t len);

    libspirv::MutableBytecode& code;
    libspirv::MutableBytecode  vert;

    // meslet builtins
    uint32_t idMeshPerVertexNV    = 0;
    uint32_t idGlPerVertex        = 0;
    uint32_t idPrimitiveCountNV   = 0;
    uint32_t idPrimind            = 0;
    uint32_t idPrimitiveIndicesNV = 0;
    uint32_t idLocalInvocationId  = 0;
    uint32_t idGlobalInvocationId = 0;
    uint32_t idWorkGroupID        = 0;
    uint32_t std450Import         = 0;

    std::unordered_map<uint32_t,VarItm> outVar;
  };

