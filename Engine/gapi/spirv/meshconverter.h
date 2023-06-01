#pragma once

#include <functional>
#include <unordered_set>
#include <unordered_map>

#include "libspirv/libspirv.h"

class MeshConverter {
  public:
    explicit MeshConverter(libspirv::MutableBytecode& code);

    struct Options {
      bool deferredMeshShading = false;
      };
    Options options;

    libspirv::MutableBytecode& computeShader()     { return comp; }
    libspirv::MutableBytecode& vertexPassthrough() { return vert; }

    void     exec();

  private:
    void     analyzeBuiltins();
    void     traveseVaryings();

    void     emitComp (libspirv::MutableBytecode& comp);
    void     emitVert (libspirv::MutableBytecode& vert);
    void     emitMVert(libspirv::MutableBytecode& vert);

    void     emitConstants(libspirv::MutableBytecode& comp);
    void     emitEngSsbo(libspirv::MutableBytecode& comp, spv::ExecutionModel emode);
    void     emitSetMeshOutputs(libspirv::MutableBytecode& comp, uint32_t engSetMesh,
                                uint32_t gl_LocalInvocationIndex, uint32_t gl_WorkGroupID);
    void     emitEngMain (libspirv::MutableBytecode& comp, uint32_t engMain);
    void     emitVboStore(libspirv::MutableBytecode& comp, libspirv::MutableBytecode::Iterator& gen,
                          const libspirv::Bytecode::OpCode& op, uint32_t achain);
    void     emitIboStore(libspirv::MutableBytecode& comp, libspirv::MutableBytecode::Iterator& gen,
                          const libspirv::Bytecode::OpCode& op, uint32_t achain);

    libspirv::Bytecode&        code;
    libspirv::MutableBytecode  comp, vert;

    struct Varying {
      uint32_t type        = 0;
      uint32_t writeOffset = 0;
      uint32_t vsVariable  = 0;
      uint32_t location    = uint32_t(-1);
      };

    uint32_t workGroupSize[3]               = {};
    uint32_t gl_NumWorkGroups               = 0;
    uint32_t gl_WorkGroupSize               = 0;
    uint32_t gl_WorkGroupID                 = 0;
    uint32_t gl_LocalInvocationID           = 0;
    uint32_t gl_LocalInvocationIndex        = 0;
    uint32_t gl_GlobalInvocationID          = 0;
    uint32_t gl_MeshPerVertexEXT            = 0;
    uint32_t gl_PrimitiveTriangleIndicesEXT = 0;
    uint32_t main                           = 0;

    uint32_t vIndirectCmd                   = 0;
    uint32_t vDecriptors                    = 0;
    uint32_t vScratch                       = 0;
    uint32_t vIboPtr                        = 0;
    uint32_t vVboPtr                        = 0;
    uint32_t vTmp                           = 0;

    uint32_t varCount                       = 0;

    std::unordered_map<uint32_t, size_t>  iboAccess;
    std::unordered_map<uint32_t, size_t>  vboAccess;
    std::unordered_map<uint32_t, Varying> varying;
    std::vector<uint32_t>                 constants;
  };

