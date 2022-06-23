#include "libspirv.h"

using namespace libspirv;


Bytecode::Bytecode(const uint32_t* spirv, size_t codeLen)
  :spirv(reinterpret_cast<const OpCode*>(spirv)), codeLen(codeLen) {
  static_assert(sizeof(OpCode)==4, "spirv instructions are 4 bytes long");
  }

Bytecode::Iterator Bytecode::begin() const {
  // skip header by adding 5
  // see: 2.3. Physical Layout of a SPIR-V Module and Instruction
  Iterator it{spirv+5};
  return it;
  }

Bytecode::Iterator Bytecode::end() const {
  Iterator it{spirv+codeLen};
  return it;
  }
