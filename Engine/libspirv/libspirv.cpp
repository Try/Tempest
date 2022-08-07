#include "libspirv.h"
#include <cassert>

using namespace libspirv;

Bytecode::Bytecode(const uint32_t* spirv, size_t codeLen)
  :spirv(reinterpret_cast<const OpCode*>(spirv)), codeLen(codeLen) {
  static_assert(sizeof(OpCode)==4, "spirv instructions are 4 bytes long");
  }

Bytecode::Iterator Bytecode::begin() const {
  Iterator it{spirv+HeaderSize, iteratorGen};
  return it;
  }

Bytecode::Iterator Bytecode::end() const {
  Iterator it{spirv+codeLen, iteratorGen};
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

bool Bytecode::isTypeDecl(spv::Op op) {
  switch(op) {
    case spv::OpTypeVoid:
    case spv::OpTypeBool:
    case spv::OpTypeInt:
    case spv::OpTypeFloat:
    case spv::OpTypeVector:
    case spv::OpTypeMatrix:
    case spv::OpTypeImage:
    case spv::OpTypeSampler:
    case spv::OpTypeSampledImage:
    case spv::OpTypeArray:
    case spv::OpTypeRuntimeArray:
    case spv::OpTypeStruct:
    case spv::OpTypeOpaque:
    case spv::OpTypePointer:
    case spv::OpTypeFunction:
    case spv::OpTypeEvent:
    case spv::OpTypeDeviceEvent:
    case spv::OpTypeReserveId:
    case spv::OpTypeQueue:
    case spv::OpTypePipe:
    case spv::OpTypeForwardPointer:
      return true;
    default:
      break;
    }
  return false;
  }

bool Bytecode::isBasicTypeDecl(spv::Op op) {
  switch(op) {
    case spv::OpTypeVoid:
    case spv::OpTypeBool:
    case spv::OpTypeInt:
    case spv::OpTypeFloat:
      return true;
    default:
      break;
    }
  return false;
  }

uint32_t Bytecode::spirvVersion() const {
  return reinterpret_cast<const uint32_t&>(spirv[1]);
  }


MutableBytecode::MutableBytecode(const uint32_t* spirv, size_t codeLen)
  :Bytecode(nullptr,0), code(reinterpret_cast<const OpCode*>(spirv),reinterpret_cast<const OpCode*>(spirv)+codeLen) {
  code.reserve(1024);
  Bytecode::spirv   = code.data();
  Bytecode::codeLen = code.size();
  }

static const uint32_t header[5] = {0x07230203, 0x00010000, 0x00080001, 0x00000001, 0x00000000};
MutableBytecode::MutableBytecode()
  :MutableBytecode(header,5){
  }

MutableBytecode::Iterator MutableBytecode::begin() {
  Iterator it{this, spirv+HeaderSize, iteratorGen};
  return it;
  }

MutableBytecode::Iterator MutableBytecode::end() {
  Iterator it{this, spirv+codeLen, iteratorGen};
  return it;
  }

MutableBytecode::Iterator MutableBytecode::fromOffset(size_t off) {
  Iterator it{this, spirv+off, iteratorGen};
  return it;
  }

MutableBytecode::Iterator MutableBytecode::findOpEntryPoint(spv::ExecutionModel em, std::string_view destName) {
  for(auto it = begin(), end = this->end(); it!=end; ++it) {
    auto& i = *it;
    if(i.op()!=spv::OpEntryPoint || i[1]!=em)
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
  auto ret = begin, e = end();
  while(ret!=e) {
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

uint32_t MutableBytecode::OpTypeVoid(Iterator& typesEnd) {
  for(auto it=begin(); it!=typesEnd; ++it) {
    auto& i = *it;
    if(i.op()==spv::OpTypeVoid)
      return i[1];
    }
  const uint32_t tRet = fetchAddBound();
  typesEnd.insert(spv::OpTypeVoid,{tRet});
  return tRet;
  }

uint32_t MutableBytecode::OpTypeBool(Iterator& typesEnd) {
  for(auto it=begin(); it!=typesEnd; ++it) {
    auto& i = *it;
    if(i.op()==spv::OpTypeBool)
      return i[1];
    }
  const uint32_t tRet = fetchAddBound();
  typesEnd.insert(spv::OpTypeBool,{tRet});
  return tRet;
  }

uint32_t MutableBytecode::OpTypeInt(Iterator& typesEnd, uint16_t bitness, bool sign) {
  uint32_t s = (sign ? 1 :0);
  for(auto it=begin(); it!=typesEnd; ++it) {
    auto& i = *it;
    if(i.op()==spv::OpTypeInt && i[2]==bitness && i[3]==s)
      return i[1];
    }
  const uint32_t tRet = fetchAddBound();
  typesEnd.insert(spv::OpTypeInt, {tRet, bitness, s});
  return tRet;
  }

uint32_t MutableBytecode::OpTypeFloat(Iterator& typesEnd, uint16_t bitness) {
  for(auto it=begin(); it!=typesEnd; ++it) {
    auto& i = *it;
    if(i.op()==spv::OpTypeFloat && i[2]==bitness)
      return i[1];
    }
  const uint32_t tRet = fetchAddBound();
  typesEnd.insert(spv::OpTypeFloat, {tRet, bitness});
  return tRet;
  }

uint32_t MutableBytecode::OpTypeVector(Iterator& typesEnd, uint32_t eltType, uint32_t size) {
  for(auto it=begin(); it!=typesEnd; ++it) {
    auto& i = *it;
    if(i.op()==spv::OpTypeVector && i[2]==eltType && i[3]==size)
      return i[1];
    }
  const uint32_t tRet = fetchAddBound();
  typesEnd.insert(spv::OpTypeVector, {tRet, eltType, size});
  return tRet;
  }

uint32_t MutableBytecode::OpTypeMatrix(Iterator& typesEnd, uint32_t eltType, uint32_t size) {
  for(auto it=begin(); it!=typesEnd; ++it) {
    auto& i = *it;
    if(i.op()==spv::OpTypeMatrix && i[2]==eltType && i[3]==size)
      return i[1];
    }
  const uint32_t tRet = fetchAddBound();
  typesEnd.insert(spv::OpTypeMatrix, {tRet, eltType, size});
  return tRet;
  }

uint32_t MutableBytecode::OpTypeArray(Iterator& typesEnd, uint32_t eltType, uint32_t size) {
  for(auto it=begin(); it!=typesEnd; ++it) {
    auto& i = *it;
    if(i.op()==spv::OpTypeArray && i[2]==eltType && i[3]==size)
      return i[1];
    }
  const uint32_t tRet = fetchAddBound();
  typesEnd.insert(spv::OpTypeArray, {tRet, eltType, size});
  return tRet;
  }

uint32_t MutableBytecode::OpTypeRuntimeArray(Iterator& typesEnd, uint32_t eltType) {
  // NOTE: spirv allows for multiple declarations of array with different strides(specifyed by Decoration)
  /*
  for(auto it=begin(); it!=typesEnd; ++it) {
    auto& i = *it;
    if(i.op()==spv::OpTypeRuntimeArray && i[2]==eltType)
      ;//return i[1];
    }
  */
  const uint32_t tRet = fetchAddBound();
  typesEnd.insert(spv::OpTypeRuntimeArray, {tRet, eltType});
  return tRet;
  }

uint32_t MutableBytecode::OpTypePointer(Iterator& typesEnd, spv::StorageClass cls, uint32_t eltType) {
  for(auto it=begin(); it!=typesEnd; ++it) {
    auto& i = *it;
    if(i.op()==spv::OpTypePointer && i[2]==cls && i[3]==eltType)
      return i[1];
    }
  const uint32_t tRet = fetchAddBound();
  typesEnd.insert(spv::OpTypePointer, {tRet, uint32_t(cls), eltType});
  return tRet;
  }

uint32_t MutableBytecode::OpTypeStruct(Iterator& typesEnd, std::initializer_list<uint32_t> member) {
  return OpTypeStruct(typesEnd, member.begin(), member.size());
  }

uint32_t MutableBytecode::OpTypeStruct(Iterator& typesEnd, const uint32_t* member, const size_t size) {
  for(auto it=begin(); it!=typesEnd; ++it) {
    auto& i = *it;
    if(i.op()!=spv::OpTypeStruct || i.len!=size+2)
      continue;
    if(std::memcmp(&i[2],member,size*4)!=0)
      break;
    return i[1];
    }

  const uint32_t tRet = fetchAddBound();
  uint16_t len = uint16_t(size + 2);
  OpCode   cx  = {uint16_t(spv::OpTypeStruct),len};
  size_t   at  = std::distance(static_cast<const OpCode*>(code.data()), typesEnd.code);

  code.resize(code.size()+cx.len);
  for(size_t i=code.size()-cx.len; i>at;) {
    --i;
    code[i+cx.len] = code[i];
    }
  code[at+0] = cx;
  code[at+1] = reinterpret_cast<const OpCode&>(tRet);
  for(size_t i=0; i<size; ++i) {
    reinterpret_cast<uint32_t&>(code[at+i+2]) = *(member+i);
    }
  typesEnd.invalidateIterator(at+len);
  return tRet;
  }

uint32_t MutableBytecode::OpTypeFunction(Iterator& typesEnd, uint32_t idRet) {
  for(auto it=begin(); it!=typesEnd; ++it) {
    auto& i = *it;
    if(i.op()==spv::OpTypeFunction && i[2]==idRet && i.len==3)
      return i[1];
    }
  const uint32_t tFn = fetchAddBound();
  typesEnd.insert(spv::OpTypeFunction,{tFn,idRet});
  return tFn;
  }

uint32_t MutableBytecode::OpConstant(Iterator& typesEnd, uint32_t idType, uint32_t u32) {
  const uint32_t ret = fetchAddBound();
  typesEnd.insert(spv::OpConstant, {idType, ret, u32});
  return ret;
  }

uint32_t MutableBytecode::OpConstant(Iterator& typesEnd, uint32_t idType, int32_t i32) {
  return OpConstant(typesEnd,idType,reinterpret_cast<uint32_t&>(i32));
  }

uint32_t MutableBytecode::OpVariable(Iterator& fn, uint32_t idType, spv::StorageClass cls) {
  const uint32_t ret = fetchAddBound();
  fn.insert(spv::OpVariable, {idType, ret, uint32_t(cls)});
  return ret;
  }

void MutableBytecode::removeNops() {
  size_t cnt = HeaderSize;
  OpCode* i = &code.data()[HeaderSize], *dest=i, *end = code.data()+code.size();

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

void MutableBytecode::traverseType(uint32_t typeId, std::function<void (const AccessChain*, uint32_t)> fn, TraverseMode mode) {
  TraverseContext ctx = {
    findSection(libspirv::Bytecode::S_Types),
    findSectionEnd(libspirv::Bytecode::S_Types),
    mode,
    };

  AccessChain ac[256] = {};
  implTraverseType(ctx, typeId, ac, 0, fn);
  }

void MutableBytecode::implTraverseType(TraverseContext& ctx, uint32_t typeId,
                                       AccessChain* ac, const uint32_t acLen,
                                       std::function<void (const AccessChain*, uint32_t)>& fn) {
  for(auto it = ctx.typesB; it!=ctx.typesE; ++it) {
    auto& i = *it;
    if(!isTypeDecl(i.op()))
      continue;
    if(i[1]!=typeId)
      continue;
    ac[acLen].type  = &i;
    ac[acLen].index = 0;

    if(ctx.mode==TraverseMode::T_PreOrder)
      fn(ac,acLen+1);

    switch(i.op()) {
      case spv::OpTypeVoid:
      case spv::OpTypeBool:
      case spv::OpTypeInt:
      case spv::OpTypeFloat:
        break;
      case spv::OpTypeVector:
      case spv::OpTypeMatrix: {
        for(uint32_t r=0; r<i[3]; ++r) {
          ac[acLen].index = r;
          implTraverseType(ctx,i[2],ac,acLen+1,fn);
          }
        break;
        }
      case spv::OpTypeImage:
      case spv::OpTypeSampler:
      case spv::OpTypeSampledImage:
        break;
      case spv::OpTypeArray: {
        uint32_t len = 1;
        for(auto rt = ctx.typesB; rt!=ctx.typesE; ++rt) {
          auto& r = *rt;
          if(r.op()==spv::OpConstant && r[2]==i[3]) {
            len = r[3];
            break;
            }
          }
        for(uint32_t r=0; r<len; ++r) {
          ac[acLen].index = r;
          implTraverseType(ctx,i[2],ac,acLen+1,fn);
          }
        break;
        }
      case spv::OpTypeRuntimeArray: {
        implTraverseType(ctx,i[2],ac,acLen+1,fn);
        break;
        }
      case spv::OpTypeStruct:
        for(uint32_t r=2; r<i.length(); ++r) {
          ac[acLen].index = (r-2);
          implTraverseType(ctx,i[r],ac,acLen+1,fn);
          }
        break;
      case spv::OpTypeOpaque:
        break;
      case spv::OpTypePointer: {
        implTraverseType(ctx,i[3],ac,acLen+1,fn);
        break;
        }
      case spv::OpTypeFunction:
      case spv::OpTypeEvent:
      case spv::OpTypeDeviceEvent:
      case spv::OpTypeReserveId:
      case spv::OpTypeQueue:
      case spv::OpTypePipe:
      case spv::OpTypeForwardPointer:
        assert(false);
        break;
      default:
        break;
      }

    if(ctx.mode==TraverseMode::T_PostOrder)
      fn(ac,acLen+1);
    break;
    }
  }

void MutableBytecode::invalidateSpvPointers() {
  spirv   = code.data();
  codeLen = code.size();
  ++iteratorGen;
  }

size_t MutableBytecode::Iterator::toOffset() const {
  assert(gen==owner->iteratorGen);
  return std::distance(static_cast<const OpCode*>(owner->code.data()), code);
  }

void MutableBytecode::Iterator::invalidateIterator(size_t at) {
  owner->invalidateSpvPointers();
  code = owner->code.data()+at;
  gen  = owner->iteratorGen;
  }

void MutableBytecode::Iterator::setToNop() {
  assert(gen==owner->iteratorGen);
  size_t at = std::distance(static_cast<const OpCode*>(owner->code.data()), code);
  auto   l  = code->len;

  auto mut = &owner->code[at];
  for(uint16_t i=0; i<l; ++i) {
    mut[i].code = spv::OpNop;
    mut[i].len  = 1;
    }
  }

void MutableBytecode::Iterator::set(uint16_t id, uint32_t c) {
  assert(gen==owner->iteratorGen);
  size_t at  = std::distance(static_cast<const OpCode*>(owner->code.data()), code);
  auto&  mut = owner->code[at+id];
  reinterpret_cast<uint32_t&>(mut) = c;
  }

void MutableBytecode::Iterator::set(uint16_t id, OpCode c) {
  assert(gen==owner->iteratorGen);
  size_t at  = std::distance(static_cast<const OpCode*>(owner->code.data()), code);
  auto&  mut = owner->code[at+id];
  mut = c;
  }

void MutableBytecode::Iterator::append(uint32_t op) {
  size_t at  = std::distance(static_cast<const OpCode*>(owner->code.data()), code);
  size_t end = at + owner->code[at].len;

  owner->code[at].len += 1;
  if(end<owner->code.size() && owner->code[end].code==spv::OpNop) {
    owner->code[end] = reinterpret_cast<OpCode&>(op);
    return;
    }

  owner->code.insert(owner->code.begin()+int(end), reinterpret_cast<OpCode&>(op));
  invalidateIterator(at);
  }

void MutableBytecode::Iterator::insert(OpCode c) {
  assert(gen==owner->iteratorGen);
  size_t at = std::distance(static_cast<const OpCode*>(owner->code.data()), code);
  owner->code.insert(owner->code.begin()+at,c);
  invalidateIterator(at+1);
  }

void MutableBytecode::Iterator::insert(spv::Op op, const uint32_t* args, const size_t argsSize) {
  assert(gen==owner->iteratorGen);
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

  invalidateIterator(at+len);
  }

void MutableBytecode::Iterator::insert(spv::Op op, std::initializer_list<uint32_t> args) {
  insert(op, args.begin(), args.size());
  }

void MutableBytecode::Iterator::insert(spv::Op op, uint32_t id, const char* s) {
  assert(gen==owner->iteratorGen);
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
  assert(gen==owner->iteratorGen);
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

void MutableBytecode::Iterator::insert(const Bytecode& block) {
  assert(gen==owner->iteratorGen);
  size_t at  = std::distance(static_cast<const OpCode*>(owner->code.data()), code);
  size_t len = block.size()-HeaderSize;
  auto&  c   = owner->code;

  c.resize(c.size()+len);
  for(size_t i=c.size()-len; i>at;) {
    --i;
    c[i+len] = c[i];
    }

  for(size_t i=0; i<len; ++i) {
    c[at+i] = block.spirv[i+HeaderSize];
    }

  invalidateIterator(at+len);
  }

