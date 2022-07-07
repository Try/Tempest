#pragma once

#include "spirv.hpp"

#include <cstring>
#include <cstdint>
#include <vector>
#include <string_view>

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
          return l.code==r.code;
          }

        friend bool operator != (const Iterator& l, const Iterator& r) {
          return l.code!=r.code;
          }

      private:
        Iterator(const OpCode* code):code(code){}
        const OpCode* code = nullptr;

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

    Iterator begin() const;
    Iterator end() const;
    size_t   size() const { return codeLen; }
    
    spv::ExecutionModel findExecutionModel() const;
    uint32_t            spirvVersion() const;

  protected:
    enum {
      // see: 2.3. Physical Layout of a SPIR-V Module and Instruction
      HeaderSize = 5
    };
    const OpCode* spirv   = nullptr;
    size_t        codeLen = 0;
  };

class MutableBytecode : public Bytecode {
  public:
    MutableBytecode(const uint32_t* spirv, size_t codeLen);

    struct Iterator : Bytecode::Iterator {
      public:
        void setToNop();
        void set(uint16_t id, uint32_t code);
        void set(uint16_t id, OpCode   code);

        void insert(OpCode code);
        void insert(spv::Op op, const uint32_t* args, size_t argsSize);
        void insert(spv::Op op, std::initializer_list<uint32_t> args);
        void insert(spv::Op op, uint32_t id, const char* annotation);
        void insert(spv::Op op, uint32_t id0, uint32_t id1, const char* annotation);

      private:
        Iterator(MutableBytecode* owner, const OpCode* code):Bytecode::Iterator(code), owner(owner){}
        MutableBytecode* owner = nullptr;

      friend class MutableBytecode;
      };

    const uint32_t* opcodes() const { return reinterpret_cast<const uint32_t*>(code.data()); }

    Iterator begin();
    Iterator end();

    Iterator fineEntryPoint(spv::ExecutionModel em, std::string_view name);

    Iterator findSection(Section s);
    Iterator findSection(Iterator begin, Section s);

    Iterator findSectionEnd(Section s);

    // Section 10 of logical layout, or 11 if 10 is absent, or 'end()'
    Iterator findFunctionsDeclBlock();

    void     removeNops();
    uint32_t fetchAddBound();

  private:
    std::vector<OpCode> code;
    void                invalidateSpvPointers();
  };
}
