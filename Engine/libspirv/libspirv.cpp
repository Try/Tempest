#include "libspirv.h"

using namespace libspirv;

Bytecode::Bytecode(const uint32_t* spirv, size_t codeLen)
  :spirv(reinterpret_cast<const OpCode*>(spirv)), codeLen(codeLen) {
  static_assert(sizeof(OpCode)==4, "spirv instructions are 4 bytes long");
  }

Bytecode::Iterator Bytecode::begin() const {
  Iterator it{spirv+HeaderSize};
  return it;
  }

Bytecode::Iterator Bytecode::end() const {
  Iterator it{spirv+codeLen};
  return it;
  }

spv::ExecutionModel Bytecode::findExecutionModel() const {
  for(auto& i:*this) {
    if(i.op()!=spv::OpEntryPoint)
      continue;
    return spv::ExecutionModel(i[1]);
    }
  return spv::ExecutionModelMax;
  }

uint32_t Bytecode::spirvVersion() const {
  return reinterpret_cast<const uint32_t&>(spirv[1]);
  }


MutableBytecode::MutableBytecode(const uint32_t* spirv, size_t codeLen)
  :Bytecode(nullptr,0), code(reinterpret_cast<const OpCode*>(spirv),reinterpret_cast<const OpCode*>(spirv)+codeLen) {
  Bytecode::spirv   = code.data();
  Bytecode::codeLen = code.size();
  }

MutableBytecode::Iterator MutableBytecode::begin() {
  Iterator it{this, spirv+HeaderSize};
  return it;
  }

MutableBytecode::Iterator MutableBytecode::end() {
  Iterator it{this, spirv+codeLen};
  return it;
  }

MutableBytecode::Iterator MutableBytecode::fineEntryPoint(spv::ExecutionModel em, std::string_view destName) {
  for(auto it = begin(), end = this->end(); it!=end; ++it) {
    auto& i = *it;
    if(i.op()!=spv::OpEntryPoint)
      continue;
    std::string_view name = reinterpret_cast<const char*>(&i[3]);
    if(name!=destName)
      continue;
    return it;
    }
  return end();
  }

MutableBytecode::Iterator MutableBytecode::findSection(Section s) {
  return findSection(begin(),s);
  }

MutableBytecode::Iterator MutableBytecode::findSection(Iterator begin, Section s) {
  auto ret = begin;
  while(ret!=end()) {
    const auto op = ret.code->code;
    // 1: capability instructions
    if(s<=S_Capability && op==spv::OpCapability)
      return ret;
    // 2: extensions to SPIR-V
    if(s<=S_Extension && op==spv::OpExtension)
      return ret;
    // 3: import instructions
    if(s<=S_ExtInstImport && op==spv::OpExtInstImport)
      return ret;
    // 4: memory model instruction
    if(s<=S_MemoryModel && op==spv::OpMemoryModel)
      return ret;
    // 5: all entry point declarations
    if(s<=S_EntryPoint && op==spv::OpEntryPoint)
      return ret;
    // 6: all execution-mode declarations
    if(s<=S_ExecutionMode && (op==spv::OpExecutionMode || op==spv::OpExecutionModeId))
      return ret;
    // 7: debug instructions
    if(s<=S_Debug && ((spv::OpSourceContinued<=op && op<=spv::OpLine) || op==spv::OpNoLine || op==spv::OpModuleProcessed))
      return ret;
    // 8: decorations block
    if(s<=S_Annotations && ((spv::OpDecorate<=op && op<=spv::OpGroupMemberDecorate) ||
                            op==spv::OpDecorateId || op==spv::OpDecorateString || op==spv::OpMemberDecorateString))
      return ret;
    // 9: types block
    if(s<=S_Types && ((spv::OpTypeVoid<=op && op<=spv::OpTypeForwardPointer) ||
                      op==spv::OpTypePipeStorage || op==spv::OpTypeNamedBarrier || op==spv::OpTypeBufferSurfaceINTEL || op==spv::OpTypeStructContinuedINTEL))
      return ret;
    // 10/11: functions block
    if(s<=S_FuncDefinitions && op==spv::OpFunction)
      return ret;
    ++ret;
    }
  return ret;
  }

MutableBytecode::Iterator MutableBytecode::findSectionEnd(Section s) {
  auto fn = findSection(s);
  fn      = findSection(fn,libspirv::Bytecode::Section(s+1));
  return fn;
  }

MutableBytecode::Iterator MutableBytecode::findFunctionsDeclBlock() {
  auto ret = begin();
  while(ret!=end()) {
    if(ret.code->code==spv::OpFunction)
      return ret;
    ++ret;
    }
  return ret;
  }

void MutableBytecode::removeNops() {
  size_t cnt = HeaderSize;
  OpCode *i=&code.data()[HeaderSize], *dest=i, *end = code.data()+code.size();

  while(i!=end) {
    if(i->code==spv::OpNop) {
      i += i->len;
      continue;
      }

    uint16_t len = i->len;
    for(uint16_t r=0; r<len; ++r) {
      dest[r] = i[r];
      }

    dest += len;
    cnt  += len;
    i    += len;
    }

  code.resize(cnt);
  invalidateSpvPointers();
  }

uint32_t MutableBytecode::fetchAddBound() {
  uint32_t& v   = reinterpret_cast<uint32_t&>(code[3]);
  auto      ret = v;
  ++v;
  return ret;
  }

void MutableBytecode::invalidateSpvPointers() {
  spirv   = code.data();
  codeLen = code.size();
  }

void MutableBytecode::Iterator::setToNop() {
  size_t at = std::distance(static_cast<const OpCode*>(owner->code.data()), code);
  auto   l  = code->len;

  auto mut = &owner->code[at];
  for(uint16_t i=0; i<l; ++i) {
    mut[i].code = spv::OpNop;
    mut[i].len  = 1;
    }
  }

void MutableBytecode::Iterator::set(uint16_t id, uint32_t c) {
  size_t at  = std::distance(static_cast<const OpCode*>(owner->code.data()), code);
  auto&  mut = owner->code[at+id];
  reinterpret_cast<uint32_t&>(mut) = c;
  }

void MutableBytecode::Iterator::set(uint16_t id, OpCode c) {
  size_t at  = std::distance(static_cast<const OpCode*>(owner->code.data()), code);
  auto&  mut = owner->code[at+id];
  mut = c;
  }

void MutableBytecode::Iterator::insert(OpCode c) {
  size_t at = std::distance(static_cast<const OpCode*>(owner->code.data()), code);
  owner->code.insert(owner->code.begin()+at,c);
  owner->invalidateSpvPointers();
  }

void MutableBytecode::Iterator::insert(spv::Op op, const uint32_t* args, const size_t argsSize) {
  uint16_t len = uint16_t(argsSize + 1);
  OpCode   cx  = {uint16_t(op),len};

  size_t at = std::distance(static_cast<const OpCode*>(owner->code.data()), code);
  auto&  c  = owner->code;
  c.resize(c.size()+cx.len);
  for(size_t i=c.size()-cx.len; i>at;) {
    --i;
    c[i+cx.len] = c[i];
    }
  c[at] = cx;
  for(int i=0; i<argsSize; ++i) {
    reinterpret_cast<uint32_t&>(c[at+i+1]) = *(args+i);
    }
  owner->invalidateSpvPointers();
  code = &owner->code[at+len];
  }

void MutableBytecode::Iterator::insert(spv::Op op, std::initializer_list<uint32_t> args) {
  insert(op, args.begin(), args.size());
  }

void MutableBytecode::Iterator::insert(spv::Op op, uint32_t id, const char* s) {
  uint32_t sz = 0;
  uint32_t args[32] = {};
  args[0] = id;
  for(int i=0; i<30*4 && s[i]; ++i) {
    reinterpret_cast<char*>(args)[i+4] = s[i];
    ++sz;
    }
  sz = 1 + (sz+1+3)/4;
  insert(op, args, sz);
  }

void MutableBytecode::Iterator::insert(spv::Op op, uint32_t id0, uint32_t id1, const char* s) {
  uint32_t sz = 0;
  uint32_t args[32] = {};
  args[0] = id0;
  args[1] = id1;
  for(int i=0; i<29*4 && s[i]; ++i) {
    reinterpret_cast<char*>(args)[i+8] = s[i];
    ++sz;
    }
  sz = 2 + (sz+1+3)/4;
  insert(op, args, sz);
  }
