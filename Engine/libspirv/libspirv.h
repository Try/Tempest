#pragma once

#include "spirv.hpp"

#include <cstring>
#include <cstdint>
#include <vector>
#include <string_view>
#include <functional>
#include <cassert>

namespace libspirv {

class Bytecode {
  public:
    Bytecode(const uint32_t* spirv, size_t codeLen);

    struct OpCode {
      uint16_t code = 0;
      uint16_t len  = 0;

      spv::Op         op()     const { return spv::Op(code); }
      uint16_t        length() const { return len;           }
      const uint32_t& operator[](uint16_t id) const { return reinterpret_cast<const uint32_t*>(this)[id]; }
      };

    struct Iterator {
      public:
        Iterator() = default;

        const OpCode& operator* () const {
          return *code;
          }

        void operator ++ () {
          code += code->len;
          }

        friend bool operator == (const Iterator& l, const Iterator& r) {
          assert(l.gen==r.gen);
          return l.code==r.code;
          }

        friend bool operator != (const Iterator& l, const Iterator& r) {
          assert(l.gen==r.gen);
          return l.code!=r.code;
          }

      private:
        explicit Iterator(const OpCode* code, uint32_t gen):code(code),gen(gen){}
        const OpCode* code = nullptr;
        uint32_t      gen  = uint32_t(-1);

      friend class Bytecode;
      friend class MutableBytecode;
      };

    // 2.4. Logical Layout of a Module
    enum Section:uint8_t {
      S_None             = 0,
      S_Capability       = 1,
      S_Extension        = 2,
      S_ExtInstImport    = 3,
      S_MemoryModel      = 4,
      S_EntryPoint       = 5,
      S_ExecutionMode    = 6,
      S_Debug            = 7,
      S_Annotations      = 8,
      S_Types            = 9,
      S_FuncDeclarations = 10,
      S_FuncDefinitions  = 11,
      };

    enum TraverseMode : uint8_t {
      T_PreOrder  = 0,
      T_PostOrder = 1,
      };

    struct AccessChain {
      const OpCode* type  = nullptr;
      uint32_t      index = 0;
      };

    Iterator begin() const;
    Iterator end() const;
    size_t   size() const { return codeLen; }

    uint32_t bound() const;

    uint32_t            spirvVersion() const;
    spv::ExecutionModel findExecutionModel() const;

    Iterator            fromOffset(size_t off) const;
    size_t              toOffset(const OpCode& op) const;

    Iterator            findSection(Section s);
    Iterator            findSection(Iterator begin, Section s);
    Iterator            findSectionEnd(Section s);

    static bool         isTypeDecl(spv::Op op);
    static bool         isBasicTypeDecl(spv::Op op);

    void     traverseType(uint32_t typeId, std::function<void(const AccessChain* indexes, uint32_t len)> fn,
                          TraverseMode mode = TraverseMode::T_PreOrder);

  protected:
    enum {
      // see: 2.3. Physical Layout of a SPIR-V Module and Instruction
      HeaderSize = 5
      };

    struct TraverseContext {
      Iterator     typesB;
      Iterator     typesE;
      TraverseMode mode = TraverseMode::T_PreOrder;
      };
    void     implTraverseType(TraverseContext& ctx, uint32_t typeId,
                              AccessChain* ac, const uint32_t acLen,
                              std::function<void(const AccessChain* indexes, uint32_t len)>& fn);

    const OpCode* spirv       = nullptr;
    size_t        codeLen     = 0;
    uint32_t      iteratorGen = 0;

  friend class MutableBytecode;
  };

class MutableBytecode : public Bytecode {
  public:
    MutableBytecode(const uint32_t* spirv, size_t codeLen);
    MutableBytecode();

    struct Iterator : Bytecode::Iterator {
      public:
        void setToNop();
        void set(uint16_t id, uint32_t code);
        void set(uint16_t id, OpCode   code);
        void append(uint32_t op);

        void insert(OpCode code);
        void insert(spv::Op op, const uint32_t* args, size_t argsSize);
        void insert(spv::Op op, std::initializer_list<uint32_t> args);
        void insert(spv::Op op, uint32_t id, const char* annotation);
        void insert(spv::Op op, uint32_t id0, uint32_t id1, const char* annotation);
        void insert(const Bytecode& block);

        size_t toOffset() const;

      private:
        Iterator(MutableBytecode* owner, const OpCode* code, uint32_t gen):Bytecode::Iterator(code,gen), owner(owner){}
        MutableBytecode* owner = nullptr;

        void invalidateIterator(size_t at);

      friend class MutableBytecode;
      };

    const uint32_t* opcodes() const { return reinterpret_cast<const uint32_t*>(code.data()); }

    Iterator begin();
    Iterator end();

    Iterator fromOffset(size_t off);
    Iterator findOpEntryPoint(spv::ExecutionModel em, std::string_view name);

    Iterator findSection(Section s);
    Iterator findSection(Iterator begin, Section s);
    Iterator findSectionEnd(Section s);

    uint32_t OpTypeVoid        (Iterator& typesEnd);
    uint32_t OpTypeBool        (Iterator& typesEnd);
    uint32_t OpTypeInt         (Iterator& typesEnd, uint16_t bitness, bool sign);
    uint32_t OpTypeFloat       (Iterator& typesEnd, uint16_t bitness);
    uint32_t OpTypeVector      (Iterator& typesEnd, uint32_t eltType, uint32_t size);
    uint32_t OpTypeMatrix      (Iterator& typesEnd, uint32_t eltType, uint32_t size);
    uint32_t OpTypeArray       (Iterator& typesEnd, uint32_t eltType, uint32_t size);
    uint32_t OpTypeRuntimeArray(Iterator& typesEnd, uint32_t eltType);
    uint32_t OpTypePointer     (Iterator& typesEnd, spv::StorageClass cls, uint32_t eltType);
    uint32_t OpTypeStruct      (Iterator& typesEnd, std::initializer_list<uint32_t> member);
    uint32_t OpTypeStruct      (Iterator& typesEnd, const uint32_t* member, const size_t size);
    uint32_t OpTypeFunction    (Iterator& typesEnd, uint32_t idRet);
    uint32_t OpTypeFunction    (Iterator& typesEnd, uint32_t idRet, std::initializer_list<uint32_t> args);

    uint32_t OpConstant         (Iterator& typesEnd, uint32_t idType, uint32_t u32);
    uint32_t OpConstant         (Iterator& typesEnd, uint32_t idType, int32_t  i32);

    uint32_t OpVariable         (Iterator& fn, uint32_t idType, spv::StorageClass cls);
    uint32_t OpFunctionParameter(Iterator& fn, uint32_t idType);

    void     removeNops();
    uint32_t fetchAddBound();
    uint32_t fetchAddBound(uint32_t cnt);
    void     setSpirvVersion(uint32_t bitver);

    void     traverseType(uint32_t typeId, std::function<void(const AccessChain* indexes, uint32_t len)> fn,
                          TraverseMode mode = TraverseMode::T_PreOrder);

  private:
    std::vector<OpCode> code;
    void     invalidateSpvPointers();
  };
}
