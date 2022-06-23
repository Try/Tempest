#pragma once

#include "spirv.hpp"

#include <cstring>
#include <cstdint>

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
      };

    Iterator begin() const;
    Iterator end() const;

  private:
    const OpCode* spirv   = nullptr;
    size_t        codeLen = 0;
  };

}
