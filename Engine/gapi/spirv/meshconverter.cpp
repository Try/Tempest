#include "meshconverter.h"

MeshConverter::MeshConverter(libspirv::MutableBytecode& code):code(code) {
  }

void MeshConverter::exec() {
  analyzeBuiltins();
  traveseVaryings();
  emitComp();
  emitVert();
  }

void MeshConverter::analyzeBuiltins() {
  for(auto& i:code) {
    if(i.op()==spv::OpDecorate && i[2]==spv::DecorationBuiltIn) {
      switch(i[3]) {
        case spv::BuiltInNumWorkgroups:
          gl_NumWorkGroups = i[1];
          break;
        case spv::BuiltInWorkgroupSize:
          gl_WorkGroupSize = i[1];
          break;
        case spv::BuiltInWorkgroupId:
          gl_WorkGroupID = i[1];
          break;
        case spv::BuiltInLocalInvocationId:
          gl_LocalInvocationID = i[1];
          break;
        case spv::BuiltInLocalInvocationIndex:
          gl_LocalInvocationIndex = i[1];
          break;
        case spv::BuiltInGlobalInvocationId:
          gl_GlobalInvocationID = i[1];
          break;
        case spv::BuiltInPrimitivePointIndicesEXT:
        case spv::BuiltInPrimitiveLineIndicesEXT:
        case spv::BuiltInPrimitiveTriangleIndicesEXT:
          gl_PrimitiveTriangleIndicesEXT = i[1];
          break;
        }
      }
    if(i.op()==spv::OpMemberDecorate && i[3]==spv::DecorationBuiltIn) {
      switch(i[4]) {
        case spv::BuiltInPosition:
          gl_MeshPerVertexEXT = i[1];
          break;
        }
      }
    if(i.op()==spv::OpEntryPoint) {
      main = i[2];
      }
    }
  }

void MeshConverter::traveseVaryings() {
  for(auto& i:code) {
    switch(i.op()) {
      case spv::OpVariable: {
        if(i[3]==spv::StorageClassOutput) {
          uint32_t id = i[2];
          if(id!=gl_PrimitiveTriangleIndicesEXT) {
            Varying it;
            it.type     = i[1];
            varying[id] = it;
            }
          }
        break;
        }
      default:
        break;
      }
    }

  for(auto& i:varying) {
    i.second.writeOffset = varCount;
    // preallocate indexes
    code.traverseType(i.second.type,[&](const libspirv::Bytecode::AccessChain* ids, uint32_t len) {
      if(!code.isBasicTypeDecl(ids[len-1].type->op()))
        return;
      if(len<2 || ids[1].index!=0)
        return; // [max_vertex] arrayness
      ++varCount;
      });
    }
  }

void MeshConverter::emitComp() {
  bool ssboEmitted = false;

  comp.setSpirvVersion(code.spirvVersion());
  comp.fetchAddBound(code.bound()-comp.bound());

  const uint32_t myMain     = comp.fetchAddBound();
  const uint32_t engSetMesh = comp.fetchAddBound();

  auto gen = comp.end();

  // copy most of mesh code
  for(auto it = code.begin(); it!=code.end(); ++it) {
    auto& i = *it;
    if(i.op()==spv::OpCapability) {
      if(i[1]==spv::CapabilityMeshShadingEXT) {
        gen.insert(spv::OpCapability,{spv::CapabilityShader});
        continue;
        }
      }
    if(i.op()==spv::OpSourceExtension) {
      std::string_view name = reinterpret_cast<const char*>(&i[1]);
      if(name=="GL_EXT_mesh_shader")
        continue;
      }
    if(i.op()==spv::OpExtension) {
      std::string_view name = reinterpret_cast<const char*>(&i[1]);
      if(name=="SPV_EXT_mesh_shader")
        continue;
      }
    if(i.op()==spv::OpExecutionMode) {
      if(i[2]==spv::ExecutionModeOutputVertices) {
        continue;
        }
      if(i[2]==spv::ExecutionModeOutputPrimitivesEXT) {
        continue;
        }
      if(i[2]==spv::ExecutionModeOutputTrianglesEXT) {
        continue;
        }
      if(i[1]==main) {
        auto off = gen.toOffset();
        gen.insert(i.op(),&i[1],i.length()-1);

        auto ix = comp.fromOffset(off);
        ix.set(1,myMain);
        continue;
        }
      }
    if(i.op()==spv::OpEntryPoint) {
      uint32_t ops[128] = {};
      ops[0] = spv::OpEntryPoint;
      ops[1] = spv::ExecutionModelGLCompute;
      ops[2] = myMain;     // main id
      ops[3] = 0x6E69616D; // "main"
      ops[4] = 0x0;

      uint32_t cnt = 5;
      for(size_t r=5; r<i.length(); ++r) {
        uint32_t id = i[r];
        if(id==gl_PrimitiveTriangleIndicesEXT)
          continue;
        if(varying.find(id)!=varying.end())
          continue;
        ops[cnt] = id; ++cnt;
        }

      gen.insert(i.op(),&ops[1],cnt-1);
      continue;
      }
    if(i.op()==spv::OpDecorate && i[2]==spv::DecorationBuiltIn) {
      if(i[3]==spv::BuiltInPrimitivePointIndicesEXT ||
         i[3]==spv::BuiltInPrimitiveLineIndicesEXT ||
         i[3]==spv::BuiltInPrimitiveTriangleIndicesEXT) {
        continue;
        }
      }
    if(i.op()==spv::OpMemberDecorate && i[3]==spv::DecorationBuiltIn) {
      continue;
      }
    if(i.op()==spv::OpName) {
      auto off = gen.toOffset();
      gen.insert(i.op(),&i[1],i.length()-1);
      if(std::string_view(reinterpret_cast<const char*>(&i[2])).find("gl_")==0) {
        if(i[1]!=gl_NumWorkGroups &&
           i[1]!=gl_WorkGroupSize &&
           i[1]!=gl_WorkGroupID &&
           i[1]!=gl_LocalInvocationID &&
           i[1]!=gl_GlobalInvocationID) {
          auto ix = comp.fromOffset(off);
          uint32_t gl = (*ix)[2];
          reinterpret_cast<char*>(&gl)[1] = '1';
          ix.set(2,gl);
          }
        }
      else if(i[1]==main) {
        auto ix = comp.fromOffset(off);
        ix.set(1,myMain);
        }
      continue;
      }
    if(i.op()==spv::OpMemberName) {
      auto off = gen.toOffset();
      gen.insert(i.op(),&i[1],i.length()-1);
      if(std::string_view(reinterpret_cast<const char*>(&i[3])).find("gl_")==0) {
        auto ix = comp.fromOffset(off);
        uint32_t gl = (*ix)[3];
        reinterpret_cast<char*>(&gl)[1] = '1';
        ix.set(3,gl);
        }
      continue;
      }
    if(i.op()==spv::OpTypePointer) {
      auto off = gen.toOffset();
      gen.insert(i.op(),&i[1],i.length()-1);
      if(i[2]==spv::StorageClassOutput) {
        auto ix = comp.fromOffset(off);
        ix.set(2,spv::StorageClassWorkgroup);
        }
      continue;
      }
    if(i.op()==spv::OpVariable) {
      auto off = gen.toOffset();
      gen.insert(i.op(),&i[1],i.length()-1);
      if(i[3]==spv::StorageClassOutput) {
        auto ix = comp.fromOffset(off);
        ix.set(3,spv::StorageClassWorkgroup);
        }
      continue;
      }
    if(i.op()==spv::OpFunction) {
      if(!ssboEmitted) {
        emitConstants();
        emitEngSsbo();
        ssboEmitted = true;
        gen = comp.end();
        }
      }
    if(i.op()==spv::OpSetMeshOutputsEXT) {
      gen = comp.findSectionEnd(libspirv::Bytecode::S_Types);
      const uint32_t uint_t            = comp.OpTypeInt(gen, 32,false);
      const uint32_t ptr_Function_uint = comp.OpTypePointer(gen, spv::StorageClassFunction, uint_t);

      auto firstBlock = comp.end();
      for(; gen!=comp.end(); ++gen) {
        if((*gen).op()==spv::OpFunction)
          firstBlock = comp.end();
        if((*gen).op()==spv::OpLabel && firstBlock==comp.end()) {
          firstBlock = gen;
          ++firstBlock;
          }
        }
      gen = firstBlock;
      const uint32_t maxV = comp.fetchAddBound();
      const uint32_t maxP = comp.fetchAddBound();

      gen.insert(spv::OpVariable, {ptr_Function_uint, maxV, spv::StorageClassFunction});
      gen.insert(spv::OpVariable, {ptr_Function_uint, maxP, spv::StorageClassFunction});

      gen = comp.end();
      gen.insert(spv::OpStore, {maxV, i[1]});
      gen.insert(spv::OpStore, {maxP, i[2]});

      const uint32_t void_t = comp.OpTypeVoid(gen);
      const uint32_t tmp0   = comp.fetchAddBound();
      gen.insert(spv::OpFunctionCall, {void_t, tmp0, engSetMesh, maxV, maxP});
      continue;
      }
    if(i.op()==spv::OpAccessChain) {
      if(i[3]==gl_PrimitiveTriangleIndicesEXT) {
        iboAccess[i[2]] = code.toOffset(i);
        continue;
        }
      if(varying.find(i[3])!=varying.end()) {
        vboAccess[i[2]] = code.toOffset(i);
        continue;
        }
      }
    if(i.op()==spv::OpStore) {
      auto chain = i[1];
      if(auto it=iboAccess.find(chain); it!=iboAccess.end()) {
        emitIboStore(gen, i, it->second);
        continue;
        }
      if(auto it=vboAccess.find(chain); it!=vboAccess.end()) {
        emitVboStore(gen, i, it->second);
        continue;
        }
      }
    gen.insert(i.op(),&i[1],i.length()-1);
    }

  for(auto it = comp.begin(), end = comp.end(); it!=end; ++it) {
    auto& i = *it;
    if(i.op()==spv::OpDecorate && i[1]==gl_MeshPerVertexEXT && i[2]==spv::DecorationBlock) {
      it.setToNop();
      }

    if(i.op()==spv::OpName && (i[1]==gl_PrimitiveTriangleIndicesEXT || varying.find(i[1])!=varying.end())) {
      it.setToNop();
      }
    if(i.op()==spv::OpDecorate && (i[1]==gl_PrimitiveTriangleIndicesEXT || varying.find(i[1])!=varying.end())) {
      it.setToNop();
      }
    if(i.op()==spv::OpVariable && (i[2]==gl_PrimitiveTriangleIndicesEXT || varying.find(i[2])!=varying.end())) {
      it.setToNop();
      }
    }
  comp.removeNops();

  if(gl_LocalInvocationIndex==0) {
    auto it = comp.findSectionEnd(libspirv::Bytecode::S_Types);
    const uint32_t uint_t              = comp.OpTypeInt(it, 32, false);
    const uint32_t _ptr_Workgroup_uint = comp.OpTypePointer(it,spv::StorageClassWorkgroup, uint_t);

    it = comp.findSectionEnd(libspirv::Bytecode::S_Types);
    gl_LocalInvocationIndex = comp.OpVariable(it, _ptr_Workgroup_uint, spv::StorageClassWorkgroup);

    it = comp.findOpEntryPoint(spv::ExecutionModelGLCompute,"main");
    it.append(gl_LocalInvocationIndex);
    }

  emitSetMeshOutputs(engSetMesh);
  emitEngMain(myMain);
  }

void MeshConverter::emitConstants() {
  auto fn = comp.findSectionEnd(libspirv::Bytecode::S_Types);

  const uint32_t uint_t = comp.OpTypeInt(fn, 32,false);
  for(uint32_t i=0; i<=varCount || i<=3; ++i) {
    const uint32_t constI = comp.OpConstant(fn,uint_t,i);
    constants.push_back(constI);
    }
  }

void MeshConverter::emitEngSsbo() {
  auto fn = comp.findSectionEnd(libspirv::Bytecode::S_Types);

  const uint32_t uint_t                       = comp.OpTypeInt(fn, 32, false);
  const uint32_t _ptr_Priate_uint             = comp.OpTypePointer(fn,spv::StorageClassPrivate, uint_t);
  const uint32_t _runtimearr_uint             = comp.OpTypeRuntimeArray(fn, uint_t);

  const uint32_t EngineInternal0_t            = comp.OpTypeStruct (fn, {uint_t, _runtimearr_uint}); // descritors
  const uint32_t _ptr_Uniform_EngineInternal0 = comp.OpTypePointer(fn,spv::StorageClassStorageBuffer, EngineInternal0_t);

  const uint32_t EngineInternal1_t            = comp.OpTypeStruct (fn, {uint_t, _runtimearr_uint}); // heap
  const uint32_t _ptr_Uniform_EngineInternal1 = comp.OpTypePointer(fn,spv::StorageClassStorageBuffer, EngineInternal1_t);

  const uint32_t _ptr_Workgroup_uint          = comp.OpTypePointer(fn, spv::StorageClassWorkgroup, uint_t);

  vEngine0 = comp.OpVariable(fn, _ptr_Uniform_EngineInternal0, spv::StorageClassStorageBuffer);

  vEngine1 = comp.OpVariable(fn, _ptr_Uniform_EngineInternal1, spv::StorageClassStorageBuffer);
  vIboPtr  = comp.OpVariable(fn, _ptr_Priate_uint, spv::StorageClassPrivate);
  vVboPtr  = comp.OpVariable(fn, _ptr_Priate_uint, spv::StorageClassPrivate);
  vTmp     = comp.OpVariable(fn, _ptr_Workgroup_uint, spv::StorageClassWorkgroup);

  fn = comp.findSectionEnd(libspirv::Bytecode::S_Annotations);
  fn.insert(spv::OpDecorate,       {_runtimearr_uint, spv::DecorationArrayStride, 4});

  fn.insert(spv::OpDecorate,       {vEngine0,         spv::DecorationDescriptorSet, 1});
  fn.insert(spv::OpDecorate,       {vEngine0,         spv::DecorationBinding, 2});
  //fn.insert(spv::OpDecorate,       {EngineInternal0_t,    spv::DecorationBufferBlock});
  fn.insert(spv::OpDecorate,       {EngineInternal0_t,    spv::DecorationBlock});
  fn.insert(spv::OpMemberDecorate, {EngineInternal0_t, 0, spv::DecorationOffset, 0});
  fn.insert(spv::OpMemberDecorate, {EngineInternal0_t, 1, spv::DecorationOffset, 4});

  fn.insert(spv::OpDecorate,       {vEngine1,         spv::DecorationDescriptorSet, 1});
  fn.insert(spv::OpDecorate,       {vEngine1,         spv::DecorationBinding, 3});
  //fn.insert(spv::OpDecorate,       {EngineInternal1_t,    spv::DecorationBufferBlock});
  fn.insert(spv::OpDecorate,       {EngineInternal1_t,    spv::DecorationBlock});
  fn.insert(spv::OpMemberDecorate, {EngineInternal1_t, 0, spv::DecorationOffset, 0});
  fn.insert(spv::OpMemberDecorate, {EngineInternal1_t, 1, spv::DecorationOffset, 4});

  fn = comp.findSectionEnd(libspirv::Bytecode::S_Debug);
  fn.insert(spv::OpName,       EngineInternal0_t,    "EngineInternal0");
  fn.insert(spv::OpMemberName, EngineInternal0_t, 0, "grow");
  fn.insert(spv::OpMemberName, EngineInternal0_t, 1, "desc");

  fn.insert(spv::OpName,       EngineInternal1_t,    "EngineInternal1");
  fn.insert(spv::OpMemberName, EngineInternal1_t, 0, "grow");
  fn.insert(spv::OpMemberName, EngineInternal1_t, 1, "var");

  fn.insert(spv::OpName,       vIboPtr, "iboPtr");
  fn.insert(spv::OpName,       vVboPtr, "vboPtr");
  fn.insert(spv::OpName,       vTmp,    "wgHelper");

  fn = comp.findOpEntryPoint(spv::ExecutionModelGLCompute,"main");
  fn.append(vEngine0);
  fn.append(vEngine1);
  fn.append(vIboPtr);
  fn.append(vVboPtr);
  fn.append(vTmp);
  }

void MeshConverter::emitSetMeshOutputs(uint32_t engSetMesh) {
  auto fn = comp.findSectionEnd(libspirv::Bytecode::S_Debug);
  fn.insert(spv::OpName, engSetMesh, "OpSetMeshOutputs");

  // Types
  fn = comp.findSectionEnd(libspirv::Bytecode::S_Types);
  const uint32_t void_t              = comp.OpTypeVoid(fn);
  const uint32_t bool_t              = comp.OpTypeBool(fn);
  const uint32_t uint_t              = comp.OpTypeInt(fn, 32,false);
  const uint32_t _ptr_Storage_uint   = comp.OpTypePointer(fn, spv::StorageClassStorageBuffer, uint_t);
  const uint32_t _ptr_Function_uint  = comp.OpTypePointer(fn, spv::StorageClassFunction,      uint_t);

  const uint32_t func_void_uu        = comp.OpTypeFunction(fn,void_t,{_ptr_Function_uint, _ptr_Function_uint});

  // Constants
  const uint32_t const0              = comp.OpConstant(fn,uint_t,0);
  const uint32_t const1              = comp.OpConstant(fn,uint_t,1);
  const uint32_t const2              = comp.OpConstant(fn,uint_t,2);
  const uint32_t const3              = comp.OpConstant(fn,uint_t,3);
  const uint32_t constVertSz         = comp.OpConstant(fn,uint_t,varCount);
  const uint32_t const264            = comp.OpConstant(fn,uint_t,264);
  const uint32_t constMax            = comp.OpConstant(fn,uint_t,uint16_t(-1));

  // Function
  fn = comp.end();
  fn.insert(spv::OpFunction,         {void_t, engSetMesh, spv::FunctionControlMaskNone, func_void_uu});
  const uint32_t maxV = comp.OpFunctionParameter(fn, _ptr_Function_uint);
  const uint32_t maxP = comp.OpFunctionParameter(fn, _ptr_Function_uint);

  fn.insert(spv::OpLabel,            {comp.fetchAddBound()});

  const uint32_t rgAllocSize = comp.fetchAddBound();
  {
  const uint32_t rgMaxV = comp.fetchAddBound();
  const uint32_t rgMaxP = comp.fetchAddBound();
  fn.insert(spv::OpLoad, {uint_t, rgMaxV, maxV});
  fn.insert(spv::OpLoad, {uint_t, rgMaxP, maxP});

  const uint32_t rg0 = comp.fetchAddBound();
  fn.insert(spv::OpIMul, {uint_t, rg0, rgMaxV, constVertSz});

  const uint32_t rg1 = comp.fetchAddBound();
  fn.insert(spv::OpIMul, {uint_t, rg1, rgMaxP, const3}); // Tringles

  fn.insert(spv::OpIAdd, {uint_t, rgAllocSize, rg0, rg1});
  }

  // atomicMin(wgHelper, gl_LocalInvocationIndex);
  const uint32_t rgThreadId    = comp.fetchAddBound();
  fn.insert(spv::OpLoad, {uint_t, rgThreadId, gl_LocalInvocationIndex}); // gl_LocalInvocationIndex

  // wgHelper==gl_LocalInvocationIndex
  {
    const uint32_t cond0 = comp.fetchAddBound();
    fn.insert(spv::OpIEqual, {bool_t, cond0, rgThreadId, const0});

    const uint32_t condBlockBegin = comp.fetchAddBound();
    const uint32_t condBlockEnd   = comp.fetchAddBound();
    fn.insert(spv::OpSelectionMerge,    {condBlockEnd, spv::SelectionControlMaskNone});
    fn.insert(spv::OpBranchConditional, {cond0, condBlockBegin, condBlockEnd});
    fn.insert(spv::OpLabel,             {condBlockBegin});

    const uint32_t ptrVarDest = comp.fetchAddBound();
    fn.insert(spv::OpAccessChain, {_ptr_Storage_uint, ptrVarDest, vEngine1, const0}); //&EngineInternal0::grow

    // TODO: elect only one thread
    const uint32_t rgTmp0 = comp.fetchAddBound();
    fn.insert(spv::OpAtomicIAdd, {uint_t, rgTmp0, ptrVarDest, const1/*scope*/, const0/*semantices*/, rgAllocSize});

    fn.insert(spv::OpStore, {vTmp, rgTmp0});
    // fn.insert(spv::OpReturn,            {});

    fn.insert(spv::OpBranch,            {condBlockEnd});
    fn.insert(spv::OpLabel,             {condBlockEnd});
  }

  fn.insert(spv::OpMemoryBarrier,  {const1, const264});         // memoryBarrierShared() // needed?
  fn.insert(spv::OpControlBarrier, {const2, const2, const264}); // barrier()

  // iboPtr =
  {
  const uint32_t rg0 = comp.fetchAddBound();
  fn.insert(spv::OpLoad, {uint_t, rg0, vTmp});
  fn.insert(spv::OpStore, {vIboPtr, rg0});

  const uint32_t rgMaxP = comp.fetchAddBound();
  fn.insert(spv::OpLoad, {uint_t, rgMaxP, maxP});

  const uint32_t rg1 = comp.fetchAddBound();
  fn.insert(spv::OpIMul, {uint_t, rg1, rgMaxP, const3}); // Tringles
  const uint32_t rg2 = comp.fetchAddBound();
  fn.insert(spv::OpIAdd, {uint_t, rg2, rg0, rg1});

  fn.insert(spv::OpStore, {vVboPtr, rg2});
  }

  fn.insert(spv::OpReturn,           {});
  fn.insert(spv::OpFunctionEnd,      {});
  }

void MeshConverter::emitEngMain(uint32_t engMain) {
  auto fn = comp.findSectionEnd(libspirv::Bytecode::S_Types);

  const uint32_t void_t    = comp.OpTypeVoid(fn);
  const uint32_t func_void = comp.OpTypeFunction(fn,void_t);

  fn = comp.end();
  fn.insert(spv::OpFunction,         {void_t, engMain, spv::FunctionControlMaskNone, func_void});
  fn.insert(spv::OpLabel,            {comp.fetchAddBound()});

  const uint32_t tmp0 = comp.fetchAddBound();
  fn.insert(spv::OpFunctionCall, {void_t, tmp0, main});

  fn.insert(spv::OpReturn,           {});
  fn.insert(spv::OpFunctionEnd,      {});
  }

void MeshConverter::emitVboStore(libspirv::MutableBytecode::Iterator& gen,
                                  const libspirv::Bytecode::OpCode& op, uint32_t achain) {
  const auto val = op[2];

  // Types
  auto fn = comp.findSectionEnd(libspirv::Bytecode::S_Types);
  const uint32_t uint_t            = comp.OpTypeInt(fn, 32,false);
  const uint32_t float_t           = comp.OpTypeFloat(fn, 32);
  const uint32_t _ptr_Storage_uint = comp.OpTypePointer(fn, spv::StorageClassStorageBuffer, uint_t);
  const uint32_t _ptr_Priate_uint  = comp.OpTypePointer(fn, spv::StorageClassPrivate,       uint_t);

  // Constants
  const uint32_t const0            = constants[0];
  const uint32_t const1            = constants[1];
  const uint32_t const3            = constants[3];
  const uint32_t constVertSz       = constants[varCount];

  // Function
  const auto& chain = *code.fromOffset(achain);
  uint32_t clen = chain.length();

  uint32_t lchain[32] = {};
  std::memcpy(lchain+4, &chain[3], (clen-3)*sizeof(uint32_t));

  lchain[0] = _ptr_Storage_uint;
  // lchain[1] = comp.fetchAddBound();
  lchain[2] = vEngine1;
  lchain[3] = const1;

  gen = comp.end();
  auto regPVboBase = comp.fetchAddBound();
  gen.insert(spv::OpLoad, {uint_t, regPVboBase, vVboPtr});

  auto rg0 = comp.fetchAddBound();
  gen.insert(spv::OpIMul, {uint_t, rg0, constVertSz, chain[4]});

  auto rg1 = comp.fetchAddBound();
  gen.insert(spv::OpIAdd, {uint_t, rg1, regPVboBase, rg0});

  lchain[4] = rg1;

  const auto type     = chain[1];
  const auto variable = chain[3];
  uint32_t idx = varying[variable].writeOffset;

  for(size_t i=5; i<clen; ++i) {
    uint32_t ix = chain[i];

    auto b = code.findSection   (libspirv::Bytecode::S_Types);
    auto e = code.findSectionEnd(libspirv::Bytecode::S_Types);
    for(auto r=b; r!=e; ++r) {
      // TODO: proper subtype traversal
      auto& inst = *r;
      if(inst.code==spv::OpConstant && inst[2]==ix) {
        idx+=inst[3];
        }
      }
    }

  code.traverseType(type,[&](const libspirv::Bytecode::AccessChain* ids, uint32_t len) {
    if(!code.isBasicTypeDecl(ids[len-1].type->op()))
      return;

    const auto elemetType = (*ids[len-1].type)[1];

    auto rgWrite = comp.fetchAddBound();
    gen.insert(spv::OpIAdd, {uint_t, rgWrite, rg1, constants[idx]});
    lchain[4] = rgWrite;
    ++idx;

    lchain[1] = comp.fetchAddBound();
    gen.insert(spv::OpAccessChain, lchain, 5);

    uint32_t rchain[32] = {elemetType, 0, val, 0};
    uint32_t rlen = 2;
    for(size_t i=1; i<len; ++i) {
      rchain[2+i] = ids[i].index;
      ++rlen;
      }

    uint32_t rg2 = 0;
    if(rlen>3) {
      rg2 = comp.fetchAddBound();
      rchain[1] = rg2;
      gen.insert(spv::OpCompositeExtract, rchain, rlen);
      } else {
      rg2 = val;
      }

    auto& op = *(ids[len-1].type);
    if(op[1]!=uint_t) {
      const uint32_t rg3 = comp.fetchAddBound();
      gen.insert(spv::OpBitcast, {uint_t, rg3, rg2});
      gen.insert(spv::OpStore, {lchain[1], rg3});
      } else {
      gen.insert(spv::OpStore, {lchain[1], rg2});
      }
    });
  }

void MeshConverter::emitIboStore(libspirv::MutableBytecode::Iterator& gen,
                                  const libspirv::Bytecode::OpCode& op, uint32_t achain) {
  const auto triplet = op[2];

  // Types
  auto fn = comp.findSectionEnd(libspirv::Bytecode::S_Types);
  const uint32_t uint_t             = comp.OpTypeInt(fn, 32,false);
  const uint32_t _ptr_Storage_uint  = comp.OpTypePointer(fn, spv::StorageClassStorageBuffer, uint_t);
  const uint32_t _ptr_Priate_uint   = comp.OpTypePointer(fn, spv::StorageClassPrivate,       uint_t);

  // Constants
  const uint32_t const0             = constants[0];
  const uint32_t const1             = constants[1];
  const uint32_t const2             = constants[2];
  const uint32_t const3             = constants[3];
  const uint32_t constVertSz        = constants[varCount];

  // Function
  const auto& chain = *code.fromOffset(achain);
  uint32_t nchain[32] = {};
  uint32_t clen = chain.length();
  std::memcpy(nchain, &chain[1], (clen-1)*sizeof(uint32_t));

  nchain[0] = _ptr_Storage_uint;

  gen = comp.end();
  auto regPIboBase = comp.fetchAddBound();
  gen.insert(spv::OpLoad, {uint_t, regPIboBase, vIboPtr});

  auto rg0 = comp.fetchAddBound();
  gen.insert(spv::OpIMul, {uint_t, rg0, const3, chain[4]});

  // triangles only for now
  for(size_t i=0; i<3; ++i) {
    auto r1 = comp.fetchAddBound();
    gen.insert(spv::OpIAdd, {uint_t, r1, regPIboBase, rg0});

    auto     regPIbo = comp.fetchAddBound();
    uint32_t elts[]  = {const0, const1, const2};
    gen.insert(spv::OpIAdd, {uint_t, regPIbo, r1, elts[i]});

    nchain[1] = comp.fetchAddBound();
    nchain[2] = vEngine1;
    nchain[3] = const1;
    nchain[4] = regPIbo; // TODO: iboPtr+i
    gen.insert(spv::OpAccessChain, nchain, 5);

    auto elt = comp.fetchAddBound();
    gen.insert(spv::OpCompositeExtract, {uint_t, elt, triplet, uint32_t(i)});

    const uint32_t rg1 = comp.fetchAddBound();
    gen.insert(spv::OpIMul, {uint_t, rg1, elt, constVertSz});

    const uint32_t rg2 = comp.fetchAddBound();
    gen.insert(spv::OpLoad, {uint_t, rg2, vVboPtr});

    const uint32_t rg3 = comp.fetchAddBound();
    gen.insert(spv::OpIAdd, {uint_t, rg3, rg2, rg1});
    gen.insert(spv::OpStore, {nchain[1], rg3});
    }
  }

void MeshConverter::emitVert() {
  const uint32_t glslStd          = vert.fetchAddBound();
  const uint32_t main             = vert.fetchAddBound();
  const uint32_t lblMain          = vert.fetchAddBound();
  const uint32_t gl_VertexIndex   = vert.fetchAddBound();

  const uint32_t void_t           = vert.fetchAddBound();
  const uint32_t bool_t           = vert.fetchAddBound();
  const uint32_t int_t            = vert.fetchAddBound();
  const uint32_t uint_t           = vert.fetchAddBound();
  const uint32_t float_t          = vert.fetchAddBound();
  const uint32_t _runtimearr_uint = vert.fetchAddBound();
  const uint32_t func_t           = vert.fetchAddBound();

  auto fn = vert.end();
  fn.insert(spv::OpCapability,       {spv::CapabilityShader});
  fn.insert(spv::OpExtInstImport,    glslStd, "GLSL.std.450");
  fn.insert(spv::OpMemoryModel,      {spv::AddressingModelLogical, spv::MemoryModelGLSL450});
  {
  const uint32_t ep = fn.toOffset();
  fn.insert(spv::OpEntryPoint,       {spv::ExecutionModelVertex, main, 0x6E69616D, 0x0});
  // fn.insert(spv::OpEntryPoint,       {spv::ExecutionModelVertex, main, 0x6E69616D, 0x0, gl_VertexIndex});
  }
  fn.insert(spv::OpSource,           {spv::SourceLanguageGLSL, 450});

  /*
  fn.insert(spv::OpName,             EngineInternal3,    "EngineInternal3");
  fn.insert(spv::OpMemberName,       EngineInternal3, 0, "heap");

  fn.insert(spv::OpDecorate,         {_runtimearr_uint, spv::DecorationArrayStride, 4});
  fn.insert(spv::OpDecorate,         {EngineInternal3,  spv::DecorationBufferBlock});
  fn.insert(spv::OpMemberDecorate,   {EngineInternal3, 0, spv::DecorationOffset, 0});
  fn.insert(spv::OpDecorate,         {vEngine3, spv::DecorationDescriptorSet, 1});
  fn.insert(spv::OpDecorate,         {vEngine3, spv::DecorationBinding, 0});
  */

  fn.insert(spv::OpTypeVoid,         {void_t});
  fn.insert(spv::OpTypeBool,         {bool_t});
  fn.insert(spv::OpTypeInt,          {uint_t,  32, 0});
  fn.insert(spv::OpTypeInt,          {int_t,   32, 1});
  fn.insert(spv::OpTypeFloat,        {float_t, 32});
  fn.insert(spv::OpTypeFunction,     {func_t, void_t});
  fn.insert(spv::OpTypeRuntimeArray, {_runtimearr_uint, uint_t});

  fn.insert(spv::OpFunction,         {void_t, main, spv::FunctionControlMaskNone, func_t});
  fn.insert(spv::OpLabel,            {lblMain});
  fn.insert(spv::OpReturn,           {});
  fn.insert(spv::OpFunctionEnd,      {});
  }
