#pragma once

#include "libspirv/libspirv.h"

class ShaderAnalyzer {
  public:
    struct Function;
    struct Varying;

    explicit ShaderAnalyzer(libspirv::MutableBytecode& code);
    libspirv::MutableBytecode& vertexPassthrough() { return vert; }

    void analyze();
    void analyzeThreadMap();

    bool canGenerateVs() const;
    bool canSkipFromVs(uint32_t offset) const;

    const Function& function(uint32_t id) const;

    enum {
      MaxIndex   = 256*3,
      MaxThreads = 256,
      NoThread   = MaxThreads+1,
      };

    enum AccessBits : uint16_t {
      AC_None    = 0x0,
      AC_Const   = 0x1,
      AC_Uniform = 0x2,
      AC_Input   = 0x4,
      AC_Local   = 0x8,
      AC_Arg     = 0x10,
      AC_Global  = 0x20,
      AC_Shared  = 0x40,
      AC_Output  = 0x80,
      AC_UAV     = 0x100,
      AC_Cond    = 0x200,
      AC_All     = 0x1FF,
      };

    friend AccessBits operator | (const AccessBits a, const AccessBits b) {
      return AccessBits(uint16_t(a) | uint16_t(b));
      }

    friend void operator |= (AccessBits& a, const AccessBits b) {
      a = AccessBits(uint16_t(a) | uint16_t(b));
      }

    struct Varying {
      uint32_t type     = 0;
      uint32_t location = -1;
      };

    struct Function {
      uint32_t    codeOffset = 0;
      bool        analyzed   = false;
      AccessBits  read       = AC_None;
      AccessBits  write      = AC_None;
      bool        hasIll     = false;
      bool        barrier    = false;
      const char* dbgName    = nullptr;
      uint32_t    returnType = 0;

      bool        isPureUniform() const;
      };


    uint32_t idVoid               = 0;

    uint32_t idLocalInvocationID  = 0;
    uint32_t idGlobalInvocationID = 0;
    uint32_t idWorkGroupID        = 0;

    uint32_t idPrimitiveIndicesNV = 0;
    uint32_t idPrimitiveCountNV   = 0;
    uint32_t idMeshPerVertexNV    = 0;
    uint32_t idPerVertex          = 0;
    uint32_t idPointSize          = 0;
    uint32_t idMain               = 0;

    uint32_t localSizeX           = 0;
    uint32_t outputPrimitivesNV   = 0;
    uint32_t outputIndexes        = 0;

    uint32_t threadMapping   [MaxIndex] = {};
    uint32_t threadMappingIbo[MaxIndex] = {};

    std::unordered_map<uint32_t,Varying> varying;

  private:
    struct AccessChain {
      AccessChain() = default;
      explicit AccessChain(uint32_t ptr):chain({ptr}){}

      std::vector<uint32_t> chain;
      };

    struct Value {
      AccessBits  access   = AccessBits::AC_All;
      AccessChain pointer;

      uint32_t    value[4] = {};
      bool        hasVal   = false;

      friend Value operator | (const Value& a, const Value& b) {
        Value ret;
        ret.access = (a.access | b.access);
        return ret;
        }
      };

    struct Reg {
      spv::StorageClass cls  = spv::StorageClassGeneric;
      Value             v;

      bool     isConstOrInput() const {
        auto msk = (AC_Const | AC_Input);
        return (v.access & (~msk))==AC_None;
        }
      bool     isUniform() const {
        auto msk = (AC_Const | AC_Uniform | AC_Local | AC_Global);
        return (v.access & (~msk))==AC_None;
        }
      bool     isUniformOrInput() const {
        auto msk = (AC_Const | AC_Uniform | AC_Local | AC_Global | AC_Input);
        return (v.access & (~msk))==AC_None;
        }
      bool     isGlobalShared() const {
        auto msk = (AC_Const | AC_Uniform | AC_Local | AC_Global);
        return (v.access & (~msk))==AC_None;
        }
      };

    struct Var {
      spv::StorageClass cls = spv::StorageClassGeneric;
      Value             v;
      const char*       dbgName = nullptr;
      };

    struct Type {
      spv::Op           what   = spv::OpNop;
      spv::StorageClass cls    = spv::StorageClassGeneric;
      AccessBits        access = AccessBits::AC_None;
      };

    struct CFG {
      bool        skipFromVs = false;
      };

    static AccessBits  toAccessBits(spv::StorageClass c);
    static std::string toStr(AccessBits b);
    static std::string toStr(const Value& b);

    void     analyzeFunc (const uint32_t functionCurrent, const libspirv::Bytecode::OpCode& calee);
    void     analyzeBlock(const uint32_t functionCurrent, const libspirv::Bytecode::OpCode& calee,
                          libspirv::Bytecode::Iterator& it,
                          AccessBits acExt, const uint32_t blockId, uint32_t mergeLbl);
    void     analyzeInstr(const uint32_t functionCurrent, const libspirv::Bytecode::OpCode& op,
                          AccessBits acExt, const uint32_t blockId);
    uint32_t mappingTable(libspirv::MutableBytecode::Iterator& typesEnd, uint32_t eltType);

    uint32_t dereferenceAccessChain(const AccessChain& id);
    void     markOutputsAsThreadRelated(uint32_t elt, uint32_t thread, AccessBits bits);
    void     markIndexAsThreadRelated(uint32_t elt, uint32_t thread, AccessBits bits);

    void     makeReadAccess (const uint32_t functionCurrent, const uint32_t blockId, AccessBits ac);
    void     makeWriteAccess(const uint32_t functionCurrent, const uint32_t blockId, AccessBits ac);

    static bool isVertexFriendly(spv::StorageClass cls);

    template<class ... T>
    void     log(const T&... t);

    libspirv::MutableBytecode&            code;
    libspirv::MutableBytecode             vert;

    std::unordered_map<uint32_t,Reg>      registers;
    std::unordered_map<uint32_t,Type>     pointerTypes;
    std::unordered_map<uint32_t,Var>      variables;
    std::unordered_map<uint32_t,Function> func;
    std::unordered_map<uint32_t,CFG>      control;

    uint32_t                              currentThread = 0;
};

