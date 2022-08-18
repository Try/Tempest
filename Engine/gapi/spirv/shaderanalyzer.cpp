#include "shaderanalyzer.h"

#include <sstream>
#include <string>
#include <iostream>
#include <unordered_set>

std::string ShaderAnalyzer::toStr(ShaderAnalyzer::AccessBits b) {
  if(b==AC_All)
    return "AC_All";
  std::stringstream s;
  if(b & AC_Const)
    s << "AC_Const | ";
  if(b & AC_Uniform)
    s << "AC_Uniform | ";
  if(b & AC_Input)
    s << "AC_Input | ";
  if(b & AC_Local)
    s << "AC_Local | ";
  if(b & AC_Arg)
    s << "AC_Arg | ";
  if(b & AC_Global)
    s << "AC_Global | ";
  if(b & AC_Shared)
    s << "AC_Shared | ";
  if(b & AC_Output)
    s << "AC_Output | ";
  if(b & AC_UAV)
    s << "AC_UAV | ";
  if(b & AC_Cond)
    s << "AC_Cond | ";
  auto ret = s.str();
  if(ret.size()>3 && ret[ret.size()-2]=='|') {
    ret.resize(ret.size()-3);
    }
  if(ret.empty())
    return "AC_None";
  return ret;
  }

std::string ShaderAnalyzer::toStr(const ShaderAnalyzer::Value& b) {
  if(!b.hasVal)
    return toStr(b.access);
  std::stringstream s;
  s << toStr(b.access) << " ";
  s << b.value[0];
  return s.str();
  }

ShaderAnalyzer::AccessBits ShaderAnalyzer::toAccessBits(spv::StorageClass c) {
  switch(c) {
    case spv::StorageClassUniformConstant:
      return AC_Uniform;
    case spv::StorageClassInput:
      return AC_Input;
    case spv::StorageClassUniform:
      return AC_Uniform;
    case spv::StorageClassOutput:
      return AC_Output;
    case spv::StorageClassWorkgroup:
      return AC_Shared;
    case spv::StorageClassCrossWorkgroup:
      return AC_UAV;
    case spv::StorageClassPrivate:
      return AC_Global;
    case spv::StorageClassFunction:
      return AC_Local;
    case spv::StorageClassGeneric:
      return AC_All;
    case spv::StorageClassPushConstant:
      return AC_Uniform;
    case spv::StorageClassAtomicCounter:
      return AC_All;
    case spv::StorageClassImage:
      return AC_All;  // TODO: distinct RW and read-only
    case spv::StorageClassStorageBuffer:
      return AC_Uniform; // TODO: distinct RW and read-only
    default:
      // extensions
      return AC_All;
    }
  }

bool ShaderAnalyzer::Func::isPureUniform() const {
  auto wmsk = AC_Const | AC_Arg | AC_Local | AC_Uniform;
  auto rmsk = AC_Const | AC_Arg | AC_Local | AC_Uniform | AC_Input;
  return (write & (~wmsk))==0 && (read & (~rmsk))==0;
  }


ShaderAnalyzer::ShaderAnalyzer(libspirv::MutableBytecode& code)
  :code(code) {
  }

void ShaderAnalyzer::analyze() {
  for(auto& i:code) {
    switch(i.op()) {
      case spv::OpTypeVoid: {
        idVoid = i[1];
        break;
        }
      case spv::OpTypeBool: {
        break;
        }

      case spv::OpTypePointer: {
        uint32_t id  = i[1];
        uint32_t cls = i[2];
        pointerTypes[id].what   = i.op();
        pointerTypes[id].cls    = spv::StorageClass(cls);
        pointerTypes[id].access = toAccessBits(spv::StorageClass(cls));
        break;
        }

      case spv::OpConstantTrue:
      case spv::OpConstantFalse:{
        uint32_t id = i[2];
        Reg var = {};
        var.cls        = spv::StorageClassUniformConstant;
        var.v.access   = AC_Const;
        var.v.hasVal   = true;
        var.v.value[0] = (i.op()==spv::OpConstantTrue) ? 1 : 0;
        registers[id]  = var;
        break;
        }
      case spv::OpConstant: {
        uint32_t id = i[2];
        Reg var = {};
        var.cls        = spv::StorageClassUniformConstant;
        var.v.access   = AC_Const;
        var.v.hasVal   = true;
        var.v.value[0] = i[3];
        registers[id]  = var;
        break;
        }
      case spv::OpConstantComposite: {
        uint32_t id = i[2];
        Reg var = {};
        var.cls        = spv::StorageClassUniformConstant;
        var.v.access   = AC_Const;
        var.v.hasVal   = true;
        for(uint32_t r=3; r<i.length(); ++r)
          var.v.value[r-3] = registers[i[r]].v.value[0];
        registers[id] = var;
        break;
        }
      case spv::OpVariable: {
        uint32_t id = i[2];
        Reg var = {};
        var.cls       = spv::StorageClass(i[3]);
        var.v.access  = AC_None;
        var.v.pointer = AccessChain{id};
        registers[id] = var;

        variables[id].cls = var.cls;
        break;
        }
      case spv::OpFunctionParameter: {
        uint32_t id = i[2];
        Reg var = {};
        var.cls      = spv::StorageClassPrivate; // TODO: double check
        var.v.access = AC_Arg;
        registers[id] = var;
        break;
        }
      case spv::OpFunction: {
        auto& fn = functions[i[2]];
        fn.codeOffset = code.toOffset(i);
        fn.returnType = i[1];
        break;
        }
      // OpMemberDecorate %Inst 0 NonWritable
      default:
        break;
      }
    }

  localSizeX = 0;
  for(auto& i:code) {
    switch(i.op()) {
      case spv::OpName: {
        auto ptr = functions.find(i[1]);
        if(ptr!=functions.end()) {
          auto& fn = ptr->second;
          fn.dbgName = reinterpret_cast<const char*>(&i[2]);
          }
        auto pv = variables.find(i[1]);
        if(pv!=variables.end()) {
          auto& v = pv->second;
          v.dbgName = reinterpret_cast<const char*>(&i[2]);
          }
        break;
        }
      case spv::OpEntryPoint: {
        std::string_view name = reinterpret_cast<const char*>(&i[3]);
        if(name=="main") {
          idMain = i[2];
          }
        }
      case spv::OpDecorate: {
        if(i[2]==spv::DecorationBuiltIn && i[3]==spv::BuiltInLocalInvocationId) {
          idLocalInvocationID = i[1];
          }
        if(i[2]==spv::DecorationBuiltIn && i[3]==spv::BuiltInGlobalInvocationId) {
          idGlobalInvocationID = i[1];
          }
        if(i[2]==spv::DecorationBuiltIn && i[3]==spv::BuiltInWorkgroupId) {
          idWorkGroupID = i[1];
          }
        if(i[2]==spv::DecorationBuiltIn && i[3]==spv::BuiltInPrimitiveIndicesNV) {
          idPrimitiveIndicesNV = i[1];
          }
        if(i[2]==spv::DecorationBuiltIn && i[3]==spv::BuiltInPrimitiveCountNV) {
          idPrimitiveCountNV = i[1];
          }
        break;
        }
      case spv::OpMemberDecorate: {
        if(i[3]==spv::DecorationBuiltIn && i[4]==spv::BuiltInPosition) {
          idMeshPerVertexNV = i[1];
          }
        if(i[3]==spv::DecorationBuiltIn && i[4]==spv::BuiltInPointSize) {
          idPointSize = i[1];
          }
        break;
        }
      case spv::OpExecutionMode:{
        if(i[2]==spv::ExecutionModeLocalSize) {
          localSizeX = i[3];
          }
        break;
        }
      default:
        break;
      }
    }

  for(auto& i:code) {
    switch(i.op()) {
      case spv::OpVariable: {
        if(i[3]==spv::StorageClassOutput) {
          uint32_t id = i[2];
          if(id!=idPrimitiveCountNV && id!=idPrimitiveIndicesNV) {
            Varying it;
            it.type = i[1];
            varying.insert({id,it});
            }
          }
        break;
        }
      default:
        break;
      }
    }

  for(auto& i:code) {
    switch(i.op()) {
      case spv::OpDecorate: {
        if(i[2]==spv::DecorationLocation) {
          varying[i[1]].location = i[3];
          }
        break;
        }
      default:
        break;
      }
    }

  for(auto& i:threadMapping)
    i = NoThread;
  for(auto& i:threadMappingIbo)
    i = NoThread;

  auto ep = code.findOpEntryPoint(spv::ExecutionModelMeshNV,"main");
  // reset var
  for(uint32_t inv = 0; inv<localSizeX; ++inv) {
    currentThread                             = inv;

    variables[idLocalInvocationID].v.value[0] = inv;
    variables[idLocalInvocationID].v.value[1] = 1;
    variables[idLocalInvocationID].v.value[2] = 1;
    variables[idLocalInvocationID].v.hasVal   = true;

    for(auto& i:variables) {
      i.second.v.access = toAccessBits(spv::StorageClass(i.second.cls));
      }

    auto st = registers;
    analyzeFunc((*ep)[2],(*ep));
    registers = std::move(st);
    }
  }

void ShaderAnalyzer::analyzeFunc(const uint32_t functionCurrent, const libspirv::Bytecode::OpCode& calee) {
  auto& func = functions[functionCurrent];
  // std::cout << "func " << func.dbgName << std::endl;

  size_t prm  = 0;
  for(auto it = code.fromOffset(func.codeOffset); it!=code.end(); ++it) {
    auto& i = *it;
    switch(i.op()) {
      case spv::OpFunction:
        break;
      case spv::OpFunctionParameter: {
        uint32_t arg = calee[prm+4];
        auto& l = registers[i[2]]; // reg
        auto  r = registers[arg];  // input ptr
        // copy pointer info
        l.v = r.v;
        ++prm;
        break;
        }
      case spv::OpLabel: {
        ++it;
        // std::cout << " + block %" << i[1] << std::endl;
        analyzeBlock(functionCurrent, calee, it, AC_None, i[1], 0);
        // std::cout << " - block %" << i[1] << std::endl;
        break;
        }
      default:
        break;
      }
    if(i.op()==spv::OpLabel)
      break;
    }

  func.analyzed = true;
  // std::cout << "~" << func.dbgName << std::endl;
  }

void ShaderAnalyzer::analyzeBlock(const uint32_t functionCurrent, const libspirv::Bytecode::OpCode& calee,
                                   libspirv::Bytecode::Iterator& it, AccessBits acExt,
                                   const uint32_t blockId, uint32_t mergeLbl) {
  auto& func  = functions[functionCurrent];

  for(; it!=code.end(); ++it) {
    auto& i = *it;
    switch(i.op()) {
      case spv::OpFunctionEnd: {
        // use as 'Function Termination Instruction'
        return;
        }
      case spv::OpReturn: {
        // std::cout << " + return" << std::endl;
        break;
        }
      case spv::OpReturnValue: {
        uint32_t ret = calee[2];
        auto&    l   = registers[ret];  // reg
        auto&    r   = registers[i[1]]; // reg
        l.v = r.v;
        // std::cout << " + return " << toStr(r.v.access) << std::endl;
        break;
        }
      case spv::OpSelectionMerge: {
        uint32_t merge = i[1];
        ++it;
        auto& b = *it;
        ++it;
        if(b.op()==spv::OpBranchConditional) {
          auto  cond   = b[1];
          auto& v      = registers[cond];

          auto  access = (AC_Cond | acExt | v.v.access);
          // std::cout << "   OpBranchConditional " << toStr(access) << std::endl;
          auto& c = control[code.toOffset(i)];
          if((access & (AC_Shared|AC_Output|AC_UAV))!=AC_None) {
            c.skipFromVs = true;
            }
          if(b[2]!=b[3]) {
            // std::cout << " + block true %" << b[2] << std::endl;
            analyzeBlock(functionCurrent,calee,it,access,b[2],b[3]);
            // std::cout << "   block else %" << b[3] << std::endl;
            analyzeBlock(functionCurrent,calee,it,access,b[3],merge);
            } else {
            // std::cout << " + block true&false %" << b[2] << std::endl;
            analyzeBlock(functionCurrent,calee,it,access,b[2],merge);
            }
          // std::cout << " - block merge %" << i[1] << std::endl;
          } else {
          // switches are too difficult
          func.hasIll = true;
          }
        break;
        }
      case spv::OpLoopMerge: {
        // loops are too difficult
        func.hasIll = true;
        break;
        }
      case spv::OpLabel: {
        if(i[1]==mergeLbl)
          return;
        break;
        }
      case spv::OpFunctionCall: {
        auto& f = functions[i[3]];
        // if(!(f.analyzed && f.isPureUniform())) {
        //   }
        analyzeFunc(i[3],i);
        func.read    |= f.read;
        func.write   |= f.write;
        func.barrier |= f.barrier;
        func.hasIll  |= f.hasIll;
        break;
        }
      case spv::OpControlBarrier:
      case spv::OpMemoryBarrier: {
        //auto& block = blocks[blockId];
        //block.barrier = true;
        func.barrier  = true;
        for(auto& v:registers) {
          if(v.second.cls==spv::StorageClassWorkgroup)
            v.second.v.access = AC_Shared; // now side-effect can be observed
          }
        break;
        }
      default:
        analyzeInstr(functionCurrent,i,acExt,blockId);
        break;
      }
    }
  }

void ShaderAnalyzer::analyzeInstr(const uint32_t functionCurrent, const libspirv::Bytecode::OpCode& i,
                                   AccessBits acExt, const uint32_t blockId) {
  switch(i.op()) {
    case spv::OpLoad: {
      auto& l  = registers[i[2]]; // reg
      auto  p  = registers[i[3]]; // ptr

      auto  id = dereferenceAccessChain(p.v.pointer);
      auto& v  = variables[id];
      {
      // std::cout  << toStr(p.v.access) << " >> " << v.dbgName << std::endl;
      }

      l.v         = v.v;
      l.v.access |= p.v.access;

      if(l.isConstOrInput() && v.v.hasVal) {
        //std::cout << toStr(p.v);
        //std::cout << " >> " << v.dbgName << std::endl;
        }

      makeReadAccess(functionCurrent, blockId, l.v.access);
      break;
      }
    case spv::OpStore: {
      // FIXME: control flow ?
      auto  l  = registers[i[1]]; // ptr
      auto  r  = registers[i[2]]; // reg
      auto  id = dereferenceAccessChain(l.v.pointer);

      auto& v  = variables[id];
      v.v         = r.v;
      v.v.access |= l.v.access;
      v.v.access |= acExt;

      if(v.cls==spv::StorageClassOutput)
        {
        /*
        std::cout << "   " << v.dbgName << "[";
        for(size_t t=1; t<l.v.pointer.chain.size(); ++t)
          std::cout << l.v.pointer.chain[t] <<" ";
        //std::cout << toStr(l.v);
        std::cout << "] = " << toStr(v.v.access) << std::endl;
        */
        if(l.v.pointer.chain.size()>1 && id!=idPrimitiveIndicesNV && id!=idPrimitiveCountNV) {
          markOutputsAsThreadRelated(l.v.pointer.chain[1], currentThread);
          }
        else if(l.v.pointer.chain.size()>1 && id==idPrimitiveIndicesNV) {
          markIndexAsThreadRelated(l.v.pointer.chain[1], currentThread);
          }
        }

      makeWriteAccess(functionCurrent, blockId, toAccessBits(v.cls));
      break;
      }
    case spv::OpAccessChain: {
      auto  t  = pointerTypes[i[1]]; // type
      auto& l  = registers   [i[2]]; // dest-ptr
      auto& b  = registers   [i[3]]; // base

      l.cls       = t.cls;
      l.v.access  = AC_None;
      l.v.pointer = b.v.pointer;

      for(uint16_t r=3; r<i.length(); ++r) {
        auto x  = registers[i[r]]; // reg
        auto ac = x.v.access & (~AC_Const);
        l.v.access = l.v.access | AccessBits(x.v.access);
        if(r==3)
          continue;
        if(x.v.hasVal)
          l.v.pointer.chain.push_back(x.v.value[0]); else
          l.v.pointer.chain.push_back(uint32_t(-1));
        }

      makeWriteAccess(functionCurrent, blockId, t.access);
      break;
      }
    case spv::OpCompositeConstruct:{
      auto& l = registers [i[2]]; // reg
      l.v.access = AC_None;
      for(uint16_t r=3; r<i.length(); ++r) {
        auto x = registers[i[r]]; // reg
        l.v.access = l.v.access | x.v.access;
        }
      break;
      }
    case spv::OpCompositeExtract:{
      auto& l = registers[i[2]];
      auto  r = registers[i[3]]; // reg
      l.v     = r.v;
      break;
      }
    case spv::OpIAdd:
    case spv::OpISub:
    case spv::OpIMul:
    case spv::OpIEqual:
    case spv::OpINotEqual:{
      auto& l = registers[i[2]]; // reg
      auto  a = registers[i[3]]; // a
      auto  b = registers[i[4]]; // b
      l.v.access = (a.v.access | b.v.access);

      if(a.v.hasVal && b.v.hasVal) {
        l.v.hasVal = true;
        switch(i.op()) {
          case spv::OpIAdd:
            l.v.value[0] = a.v.value[0] + b.v.value[0];
            break;
          case spv::OpISub:
            l.v.value[0] = a.v.value[0] - b.v.value[0];
            break;
          case spv::OpIMul:
            l.v.value[0] = a.v.value[0] * b.v.value[0];
            break;
          case spv::OpIEqual:
            l.v.value[0] = (a.v.value[0] == b.v.value[0]) ? 1 : 0;
            break;
          case spv::OpINotEqual:
            l.v.value[0] = (a.v.value[0] != b.v.value[0]) ? 1 : 0;
            break;
          default:break;
          }
        }
      // std::cout << " * " << toStr(l.v)<< std::endl;
      break;
      }
    case spv::OpFAdd:
    case spv::OpFSub:
    case spv::OpFMul:
    case spv::OpUDiv:
    case spv::OpSDiv:
    case spv::OpFDiv:
    case spv::OpUMod:
    case spv::OpSMod:
    case spv::OpFMod: {
      auto& l = registers[i[2]]; // reg
      auto  a = registers[i[3]]; // a
      auto  b = registers[i[4]]; // b
      l.v.access = (a.v.access | b.v.access);

      // std::cout << toStr(l.v) << " = " << toStr(a.v) << " ** " << toStr(b.v) << std::endl;
      break;
      }
    case spv::OpVectorTimesScalar:
    case spv::OpMatrixTimesScalar:
    case spv::OpVectorTimesMatrix:
    case spv::OpMatrixTimesVector:
    case spv::OpMatrixTimesMatrix: {
      auto& l = registers[i[2]]; // reg
      auto  a = registers[i[3]]; // a
      auto  b = registers[i[4]]; // b
      l.v.access = (a.v.access | b.v.access);
      break;
      }
    case spv::OpBitcast:{
      auto& l = registers[i[2]];
      auto  r = registers[i[3]];
      l.v = r.v;
      break;
      }
    default: {
      if(functionCurrent!=0) {
        auto& f = functions[functionCurrent];
        f.hasIll = true;
        // reset registers info maybe?
        }
      break;
      }
    }
  }

uint32_t ShaderAnalyzer::dereferenceAccessChain(const AccessChain& id) {
  return id.chain[0];
  }

void ShaderAnalyzer::markOutputsAsThreadRelated(uint32_t elt, uint32_t thread) {
  if(elt==uint32_t(-1)) {
    // not a constant-uniform array index
    for(auto& t:threadMapping) {
      if(t==NoThread || t==currentThread)
        t = currentThread; else
        t = MaxThreads;
      }
    return;
    }

  auto& t = threadMapping[elt];
  if(t==NoThread || t==currentThread)
    t = currentThread; else
    t = MaxThreads;
  }

void ShaderAnalyzer::markIndexAsThreadRelated(uint32_t elt, uint32_t thread) {
  if(elt==uint32_t(-1)) {
    // not a constant-uniform array index
    for(auto& t:threadMappingIbo) {
      if(t==NoThread || t==currentThread)
        t = currentThread; else
        t = MaxThreads;
      }
    return;
    }

  auto& t = threadMappingIbo[elt];
  if(t==NoThread || t==currentThread)
    t = currentThread; else
    t = MaxThreads;
  }

void ShaderAnalyzer::makeReadAccess(const uint32_t functionCurrent, const uint32_t blockId, AccessBits ac) {
  auto& f = functions[functionCurrent];
  f.read |= ac;
  // auto& b = blocks[blockId];
  // b.read |= ac;
  }

void ShaderAnalyzer::makeWriteAccess(const uint32_t functionCurrent, const uint32_t blockId,
                                      AccessBits ac) {
  auto& f = functions[functionCurrent];
  f.write |= ac;
  // auto& b = blocks[blockId];
  // b.write |= ac;
  }

bool ShaderAnalyzer::isVertexFriendly(spv::StorageClass cls) {
  if(cls==spv::StorageClassWorkgroup ||
     cls==spv::StorageClassInput ||
     cls==spv::StorageClassOutput) {
    return true;
    }
  return false;
  }

uint32_t ShaderAnalyzer::mappingTable(libspirv::MutableBytecode::Iterator& fn, uint32_t eltType) {
  const uint32_t vRet          = vert.fetchAddBound();
  const uint32_t mapping_arr_t = vert.OpTypeArray(fn, eltType, localSizeX);

  uint32_t table[MaxThreads+2] = {};
  table[0] = mapping_arr_t;
  table[1] = vRet;
  for(size_t i=0; i<localSizeX; ++i)
    table[i+2] = vert.OpConstant(fn,eltType,threadMapping[i]);
  fn.insert(spv::OpConstantComposite, table, localSizeX+2);
  return vRet;
  }

void ShaderAnalyzer::generateVs() {
  vert.fetchAddBound(code.bound()-vert.bound());
  auto gen = vert.end();

  const uint32_t idVertexIndex = vert.fetchAddBound();
  std::unordered_set<uint32_t> var;
  for(auto it = code.begin(); it!=code.end(); ++it) {
    auto& i = *it;
    if(i.op()==spv::OpCapability) {
      if(i[1]==spv::CapabilityMeshShadingNV) {
        gen.insert(spv::OpCapability,{spv::CapabilityShader});
        continue;
        }
      }
    if(i.op()==spv::OpExecutionMode) {
      continue;
      }
    if(i.op()==spv::OpEntryPoint) {
      gen.insert(spv::OpEntryPoint, {spv::ExecutionModelVertex, i[2], 0x6E69616D, 0x0, idVertexIndex});
      continue;
      }
    if(i.op()==spv::OpDecorate && i[2]==spv::DecorationBuiltIn) {
      if(i[3]==spv::BuiltInPrimitiveCountNV ||
         i[3]==spv::BuiltInPrimitiveIndicesNV ||
         i[3]==spv::BuiltInLocalInvocationId ||
         i[3]==spv::BuiltInGlobalInvocationId ||
         i[3]==spv::BuiltInWorkgroupId) {
        continue;
        }
      }

    if(i.op()==spv::OpTypePointer) {
      uint32_t cls = i[2];
      if(isVertexFriendly(spv::StorageClass(cls))) {
        cls = spv::StorageClassPrivate;
        }
      gen.insert(i.op(),{i[1],cls,i[3]});
      continue;
      }
    if(i.op()==spv::OpVariable) {
      uint32_t cls = i[3];
      if(isVertexFriendly(spv::StorageClass(cls))) {
        cls = spv::StorageClassPrivate;
        }
      var.insert(i[2]);
      gen.insert(i.op(),{i[1],i[2],cls});
      continue;
      }

    if(i.op()==spv::OpFunction) {
      auto& fn = functions[i[2]];
      var.insert(i[2]);
      if(!(fn.isPureUniform() || i[2]==idMain)) {
        gen.insert(i.op(),&i[1],i.length()-1);
        while((*it).op()!=spv::OpFunctionEnd) {
          auto& i = *it;
          if(i.op()==spv::OpFunctionParameter)
            gen.insert(i.op(),&i[1],i.length()-1);
          ++it;
          }
        uint32_t id = vert.fetchAddBound();
        gen.insert(spv::OpLabel,{id});

        if(fn.returnType==idVoid) {
          gen.insert(spv::OpReturn,{});
          } else {
          uint32_t id = vert.fetchAddBound();
          gen.insert(spv::OpUndef,{fn.returnType, id});
          gen.insert(spv::OpReturnValue,{id});
          }
        gen.insert(spv::OpFunctionEnd,{});
        continue;
        }
      }

    if(i.op()==spv::OpFunctionCall) {
      //
      }

    if(i.op()==spv::OpSelectionMerge) {
      auto& c = control[code.toOffset(i)];
      if(c.skipFromVs) {
        uint32_t merge = i[1];
        for(;;++it) {
          auto& i = *it;
          if(i.op()==spv::OpLabel & i[1]==merge)
            break;
          }
        continue;
        }
      }

    gen.insert(i.op(),&i[1],i.length()-1);
    }

  for(auto it = vert.begin(); it!=vert.end(); ++it) {
    auto& i = *it;
    if(i.op()==spv::OpName) {
      if(var.find(i[1])==var.end() || i[1]==idMain)
        it.setToNop();
      std::string_view name = reinterpret_cast<const char*>(&i[2]);
      if(name=="gl_LocalInvocationID")
        ;//it.setToNop();
      }
    }

  // engine-level main
  {
  auto fn = vert.findSectionEnd(libspirv::Bytecode::S_Types);
  const uint32_t void_t            = vert.OpTypeVoid(fn);
  const uint32_t func_void         = vert.OpTypeFunction(fn, void_t);
  const uint32_t uint_t            = vert.OpTypeInt(fn, 32, false);
  const uint32_t int_t             = vert.OpTypeInt(fn, 32, true);
  const uint32_t _ptr_Input_int    = vert.OpTypePointer(fn,spv::StorageClassInput,   int_t);
  const uint32_t _ptr_Private_uint = vert.OpTypePointer(fn,spv::StorageClassPrivate, uint_t);

  const uint32_t const0            = vert.OpConstant(fn,int_t,0);
  const uint32_t const1            = vert.OpConstant(fn,int_t,1);
  const uint32_t const2            = vert.OpConstant(fn,int_t,2);
  const uint32_t const1u           = vert.OpConstant(fn,uint_t,1);
  const uint32_t const8u           = vert.OpConstant(fn,uint_t,8);
  const uint32_t const255u         = vert.OpConstant(fn,uint_t,255);
  const uint32_t mappings          = mappingTable(fn,uint_t);

  // input
  fn.insert(spv::OpVariable, {_ptr_Input_int, idVertexIndex, spv::StorageClassInput});

  // annotations
  fn = vert.findSectionEnd(libspirv::Bytecode::S_Annotations);
  fn.insert(spv::OpDecorate, {idVertexIndex, spv::DecorationBuiltIn, spv::BuiltInVertexIndex});

  fn = vert.end();
  const uint32_t engineMain = vert.fetchAddBound();
  const uint32_t lblMain    = vert.fetchAddBound();
  fn.insert(spv::OpFunction, {void_t, engineMain, spv::FunctionControlMaskNone, func_void});
  fn.insert(spv::OpLabel,    {lblMain});

  const uint32_t rAt = vert.fetchAddBound();
  fn.insert(spv::OpLoad, {int_t, rAt, idVertexIndex});
  const uint32_t rIndex = vert.fetchAddBound();
  fn.insert(spv::OpBitcast, {uint_t, rIndex, rAt});

  if(idLocalInvocationID!=0) {
    // gl_PrimitiveIndex = gl_VertexIndex & 0xFF;
    // gl_LocalInvocationID.x = tbl[gl_PrimitiveIndex];
    const uint32_t ptrIdX = vert.fetchAddBound();
    const uint32_t mod    = vert.fetchAddBound();
    const uint32_t ptrTbl = vert.fetchAddBound();
    const uint32_t tbl    = vert.fetchAddBound();

    fn.insert(spv::OpAccessChain, {_ptr_Private_uint, ptrIdX, idLocalInvocationID, const0});
    fn.insert(spv::OpBitwiseAnd,  {uint_t, mod, rIndex, const255u});

    fn.insert(spv::OpAccessChain, {_ptr_Private_uint, ptrTbl, mappings, mod});
    fn.insert(spv::OpLoad, {int_t, tbl, ptrTbl});
    fn.insert(spv::OpStore,      {ptrIdX, tbl});

    const uint32_t ptrIdY = vert.fetchAddBound();
    fn.insert(spv::OpAccessChain, {_ptr_Private_uint, ptrIdY, idLocalInvocationID, const1});
    fn.insert(spv::OpStore, {ptrIdY, const1u});

    const uint32_t ptrIdZ = vert.fetchAddBound();
    fn.insert(spv::OpAccessChain, {_ptr_Private_uint, ptrIdZ, idLocalInvocationID, const2});
    fn.insert(spv::OpStore, {ptrIdZ, const1u});
    }

  if(idWorkGroupID!=0) {
    // gl_WorkGroupID.x = gl_VertexIndex >> 8;
    const uint32_t ptrIdX = vert.fetchAddBound();
    fn.insert(spv::OpAccessChain, {_ptr_Private_uint, ptrIdX, idWorkGroupID, const0});
    const uint32_t mod = vert.fetchAddBound();
    fn.insert(spv::OpShiftRightLogical, {uint_t, mod, rIndex, const8u});
    fn.insert(spv::OpStore,             {ptrIdX, mod});

    const uint32_t ptrIdY = vert.fetchAddBound();
    fn.insert(spv::OpAccessChain, {_ptr_Private_uint, ptrIdY, idWorkGroupID, const1});
    fn.insert(spv::OpStore, {ptrIdY, const1u});

    const uint32_t ptrIdZ = vert.fetchAddBound();
    fn.insert(spv::OpAccessChain, {_ptr_Private_uint, ptrIdZ, idWorkGroupID, const2});
    fn.insert(spv::OpStore, {ptrIdZ, const1u});
    }

  const uint32_t tmp0 = vert.fetchAddBound();
  fn.insert(spv::OpFunctionCall, {void_t, tmp0, idMain});

  fn.insert(spv::OpReturn,      {});
  fn.insert(spv::OpFunctionEnd, {});

  auto ep = vert.findOpEntryPoint(spv::ExecutionModelVertex,"main");
  assert(ep!=vert.end());
  ep.set(2,engineMain);
  }

  vert.removeNops();
  }
