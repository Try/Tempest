#include "meshconverter.h"

MeshConverter::MeshConverter(libspirv::MutableBytecode& code):code(code) {
  }

void MeshConverter::exec() {
  analyzeBuiltins();
  traveseVaryings();

  if(options.deferredMeshShading) {
    emitComp(comp);
    emitMVert(vert);
    } else {
    emitComp(comp);
    emitVert(vert);
    }
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
          gl_WorkGroupID  = i[1];
          usr_WorkGroupID = i[1];
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

  for(auto& i:code) {
    if(i.op()==spv::OpDecorate && i[2]==spv::DecorationLocation) {
      for(auto& v:varying) {
        if(v.first==i[1]) {
          v.second.location = i[3];
          break;
          }
        }
      }
    }
  }

uint32_t MeshConverter::typeSizeOf(uint32_t type) {
  if(type==0)
    return 0;
  uint32_t sz = 0;
  code.traverseType(type,[&](const libspirv::Bytecode::AccessChain* ids, uint32_t len) {
    if(!code.isBasicTypeDecl(ids[len-1].type->op()))
      return;
    sz += 4;
    });
  return sz;
  }

void MeshConverter::emitComp(libspirv::MutableBytecode& comp) {
  bool ssboEmitted = false;

  remapWg = (code.findExecutionModel()==spv::ExecutionModelMeshEXT);

  comp.setSpirvVersion(code.spirvVersion());
  comp.fetchAddBound(code.bound()-comp.bound());

  const uint32_t myMain = comp.fetchAddBound();

  uint32_t engSetMesh  = 0;
  uint32_t engEmitMesh = 0;
  uint32_t engHeadFn   = 0;
  uint32_t engTailFn   = 0;
  if(options.varyingInSharedMem && code.findExecutionModel()==spv::ExecutionModelMeshEXT) {
    engHeadFn = comp.fetchAddBound();
    engTailFn = comp.fetchAddBound();
    }
  if(code.findExecutionModel()==spv::ExecutionModelTaskEXT) {
    engEmitMesh = comp.fetchAddBound();
    }
  if(code.findExecutionModel()==spv::ExecutionModelMeshEXT) {
    engSetMesh = comp.fetchAddBound();
    }

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
      if(i[2]==spv::ExecutionModeLocalSize) {
        workGroupSize[0] = i[3];
        workGroupSize[1] = i[4];
        workGroupSize[2] = i[5];
        }
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
        uint32_t id = i[uint16_t(r)];
        if(id==gl_PrimitiveTriangleIndicesEXT)
          continue;
        if(!options.varyingInSharedMem && varying.find(id)!=varying.end())
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
      // remapped
      if(code.findExecutionModel()==spv::ExecutionModelMeshEXT && i[1]==gl_WorkGroupID)
        continue;
      }
    if(i.op()==spv::OpDecorate && i[2]==spv::DecorationLocation) {
      continue;
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
        else if(remapWg && i[1]==gl_WorkGroupID) {
          auto ix = comp.fromOffset(off);
          uint32_t gl = (*ix)[2];
          reinterpret_cast<char*>(&gl)[1] = 'x';
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
      if(i[2]==spv::StorageClassOutput || i[2]==spv::StorageClassTaskPayloadWorkgroupEXT) {
        auto ix = comp.fromOffset(off);
        ix.set(2,spv::StorageClassWorkgroup);
        }
      continue;
      }
    if(i.op()==spv::OpVariable) {
      auto off = gen.toOffset();
      gen.insert(i.op(),&i[1],i.length()-1);
      if(i[3]==spv::StorageClassTaskPayloadWorkgroupEXT) {
        taskPayloadType = i[1];
        taskPayload     = i[2];
        }
      if(i[3]==spv::StorageClassOutput || i[3]==spv::StorageClassTaskPayloadWorkgroupEXT) {
        auto ix = comp.fromOffset(off);
        ix.set(3,spv::StorageClassWorkgroup);
        }
      // remapped
      if(remapWg && i[2]==gl_WorkGroupID) {
        auto ix = comp.fromOffset(off);
        ix.setToNop();
        }
      continue;
      }
    if(i.op()==spv::OpFunction) {
      if(!ssboEmitted) {
        emitConstants(comp);
        emitEngSsbo(comp, spv::ExecutionModelGLCompute);
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
    if(i.op()==spv::OpEmitMeshTasksEXT) {
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
      const uint32_t szX = comp.fetchAddBound();
      const uint32_t szY = comp.fetchAddBound();
      const uint32_t szZ = comp.fetchAddBound();

      gen.insert(spv::OpVariable, {ptr_Function_uint, szX, spv::StorageClassFunction});
      gen.insert(spv::OpVariable, {ptr_Function_uint, szY, spv::StorageClassFunction});
      gen.insert(spv::OpVariable, {ptr_Function_uint, szZ, spv::StorageClassFunction});

      gen = comp.end();
      gen.insert(spv::OpStore, {szX, i[1]});
      gen.insert(spv::OpStore, {szY, i[2]});
      gen.insert(spv::OpStore, {szZ, i[3]});

      const uint32_t void_t = comp.OpTypeVoid(gen);
      const uint32_t tmp0   = comp.fetchAddBound();
      gen.insert(spv::OpFunctionCall, {void_t, tmp0, engEmitMesh, szX, szY, szZ});
      gen.insert(spv::OpReturn,       {});
      continue;
      }
    if(i.op()==spv::OpAccessChain) {
      if(i[3]==gl_PrimitiveTriangleIndicesEXT) {
        iboAccess[i[2]] = code.toOffset(i);
        continue;
        }
      if(!options.varyingInSharedMem && varying.find(i[3])!=varying.end()) {
        vboAccess[i[2]] = code.toOffset(i);
        continue;
        }
      }
    if(i.op()==spv::OpStore) {
      auto chain = i[1];
      if(auto it=iboAccess.find(chain); it!=iboAccess.end()) {
        emitIboStore(comp, gen, i, uint32_t(it->second));
        continue;
        }
      if(auto it=vboAccess.find(chain); it!=vboAccess.end()) {
        emitVboStore(comp, gen, i, uint32_t(it->second));
        continue;
        }
      }
    gen.insert(i.op(),&i[1],i.length()-1);
    }

  uint32_t _ptr_uvec3 = 0, _ptr_Input_uvec3 = 0;
  uint32_t _ptr_uint  = 0, _ptr_Input_uint = 0;
  if(gl_WorkGroupID!=0 && remapWg) {
    auto it = comp.findSectionEnd(libspirv::Bytecode::S_Types);
    const uint32_t uint_t     = comp.OpTypeInt(it, 32, false);
    const uint32_t uvec3_t    = comp.OpTypeVector(it, uint_t, 3);

    _ptr_uint        = comp.OpTypePointer(it, spv::StorageClassPrivate, uint_t);
    _ptr_uvec3       = comp.OpTypePointer(it, spv::StorageClassPrivate, uvec3_t);

    _ptr_Input_uint  = comp.OpTypePointer(it, spv::StorageClassInput, uint_t);
    _ptr_Input_uvec3 = comp.OpTypePointer(it, spv::StorageClassInput, uvec3_t);

    it = comp.findSectionEnd(libspirv::Bytecode::S_Types);
    it.insert(spv::OpVariable, {_ptr_uvec3, gl_WorkGroupID, uint32_t(spv::StorageClassPrivate)});
    }

  // post-process
  for(auto it = comp.begin(), end = comp.end(); it!=end; ++it) {
    auto& i = *it;
    if(i.op()==spv::OpDecorate && i[1]==gl_MeshPerVertexEXT && i[2]==spv::DecorationBlock) {
      it.setToNop();
      }

    const bool rmVarying = !options.varyingInSharedMem;
    if(i.op()==spv::OpName && (i[1]==gl_PrimitiveTriangleIndicesEXT || (rmVarying && varying.find(i[1])!=varying.end()))) {
      it.setToNop();
      }
    if(i.op()==spv::OpDecorate && (i[1]==gl_PrimitiveTriangleIndicesEXT || (rmVarying && varying.find(i[1])!=varying.end()))) {
      it.setToNop();
      }
    if(i.op()==spv::OpVariable && (i[2]==gl_PrimitiveTriangleIndicesEXT || (rmVarying && varying.find(i[2])!=varying.end()))) {
      it.setToNop();
      }
    if(i.op()==spv::OpAccessChain && remapWg && i[3]==gl_WorkGroupID) {
      if(i[1]==_ptr_Input_uint)
        it.set(1, _ptr_uint);
      else if(i[1]==_ptr_Input_uvec3)
        it.set(1, _ptr_uvec3);
      }
    }
  comp.removeNops();

  uint32_t gl_LocalInvocationIndex = this->gl_LocalInvocationIndex;
  if(gl_LocalInvocationIndex==0) {
    auto it = comp.findSectionEnd(libspirv::Bytecode::S_Types);
    const uint32_t uint_t          = comp.OpTypeInt(it, 32, false);
    const uint32_t _ptr_Input_uint = comp.OpTypePointer(it,spv::StorageClassInput, uint_t);

    it = comp.findSectionEnd(libspirv::Bytecode::S_Types);
    gl_LocalInvocationIndex = comp.OpVariable(it, _ptr_Input_uint, spv::StorageClassInput);

    it = comp.findSectionEnd(libspirv::Bytecode::S_Debug);
    it.insert(spv::OpName, gl_LocalInvocationIndex, "gl_LocalInvocationIndex");

    it = comp.findSectionEnd(libspirv::Bytecode::S_Annotations);
    it.insert(spv::OpDecorate, {gl_LocalInvocationIndex, spv::DecorationBuiltIn, spv::BuiltInLocalInvocationIndex});

    it = comp.findOpEntryPoint(spv::ExecutionModelGLCompute,"main");
    it.append(gl_LocalInvocationIndex);
    }

  uint32_t gl_WorkGroupID = this->gl_WorkGroupID;
  if(gl_WorkGroupID==0 || remapWg) {
    auto it = comp.findSectionEnd(libspirv::Bytecode::S_Types);
    const uint32_t uint_t           = comp.OpTypeInt(it, 32, false);
    const uint32_t uint3_t          = comp.OpTypeVector(it, uint_t, 3);
    const uint32_t _ptr_Input_uint3 = comp.OpTypePointer(it,spv::StorageClassInput, uint3_t);

    it = comp.findSectionEnd(libspirv::Bytecode::S_Types);
    gl_WorkGroupID = comp.OpVariable(it, _ptr_Input_uint3, spv::StorageClassInput);

    it = comp.findSectionEnd(libspirv::Bytecode::S_Debug);
    it.insert(spv::OpName, gl_WorkGroupID, "gl_WorkGroupID");

    it = comp.findSectionEnd(libspirv::Bytecode::S_Annotations);
    it.insert(spv::OpDecorate, {gl_WorkGroupID, spv::DecorationBuiltIn, spv::BuiltInWorkgroupId});

    it = comp.findOpEntryPoint(spv::ExecutionModelGLCompute,"main");
    it.append(gl_WorkGroupID);
    this->gl_WorkGroupID = gl_WorkGroupID;
    }

  if(engEmitMesh!=0)
    emitEmitMeshTasks(comp, engEmitMesh, gl_LocalInvocationIndex);
  if(engSetMesh!=0)
    emitSetMeshOutputs(comp, engSetMesh, gl_LocalInvocationIndex, gl_WorkGroupID);
  if(engHeadFn!=0)
    emitPayoadLoad(comp, engHeadFn);
  if(engTailFn!=0)
    emitTailStore(comp, engTailFn, gl_LocalInvocationIndex);
  emitEngMain(comp, myMain, engHeadFn, engTailFn);
  }

void MeshConverter::emitConstants(libspirv::MutableBytecode& comp) {
  auto fn = comp.findSectionEnd(libspirv::Bytecode::S_Types);

  const uint32_t plSize = 3 + typeSizeOf(taskPayloadType)/sizeof(uint32_t);
  const uint32_t uint_t = comp.OpTypeInt(fn, 32,false);
  constants.clear();
  for(uint32_t i=0; i<=varCount || i<=plSize || i<=10; ++i) {
    const uint32_t constI = comp.OpConstant(fn,uint_t,i);
    constants.push_back(constI);
    }

  mGrow  = constants[0];
  mVar   = constants[1];

  mTaskC = constants[0];
  mMeshC = constants[1];
  mIter  = constants[2];
  mDesc  = constants[3];
  }

void MeshConverter::emitEngSsbo(libspirv::MutableBytecode& comp, spv::ExecutionModel emode) {
  auto fn = comp.findSectionEnd(libspirv::Bytecode::S_Types);

  const uint32_t int_t                        = comp.OpTypeInt(fn, 32, true);
  const uint32_t uint_t                       = comp.OpTypeInt(fn, 32, false);
  const uint32_t _ptr_Priate_uint             = comp.OpTypePointer(fn,spv::StorageClassPrivate, uint_t);
  const uint32_t _runtimearr_uint             = comp.OpTypeRuntimeArray(fn, uint_t);

  const uint32_t DrawIndexedIndirectCommand   = comp.OpTypeStruct      (fn, {uint_t, uint_t, uint_t, uint_t, uint_t, int_t, uint_t, uint_t});
  const uint32_t Descriptor                   = comp.OpTypeStruct      (fn, {uint_t, uint_t, uint_t});
  const uint32_t _runtimearr_desc             = comp.OpTypeRuntimeArray(fn, Descriptor);

  const uint32_t EngineInternal0_t            = comp.OpTypeStruct (fn, { DrawIndexedIndirectCommand }); // indirects
  const uint32_t _ptr_Storage_EngineInternal0 = comp.OpTypePointer(fn,spv::StorageClassStorageBuffer, EngineInternal0_t);

  const uint32_t EngineInternal1_t            = comp.OpTypeStruct (fn, {uint_t, uint_t, uint_t, _runtimearr_desc}); // descritors
  const uint32_t _ptr_Storage_EngineInternal1 = comp.OpTypePointer(fn,spv::StorageClassStorageBuffer, EngineInternal1_t);

  const uint32_t EngineInternal2_t            = comp.OpTypeStruct (fn, {uint_t, _runtimearr_uint}); // heap
  const uint32_t _ptr_Storage_EngineInternal2 = comp.OpTypePointer(fn,spv::StorageClassStorageBuffer, EngineInternal2_t);

  const uint32_t _ptr_Workgroup_uint          = comp.OpTypePointer(fn, spv::StorageClassWorkgroup, uint_t);

  vIndirectCmd = comp.OpVariable(fn, _ptr_Storage_EngineInternal0, spv::StorageClassStorageBuffer);
  vDecriptors  = comp.OpVariable(fn, _ptr_Storage_EngineInternal1, spv::StorageClassStorageBuffer);
  vScratch     = comp.OpVariable(fn, _ptr_Storage_EngineInternal2, spv::StorageClassStorageBuffer);

  vIboPtr      = comp.OpVariable(fn, _ptr_Priate_uint, spv::StorageClassPrivate);
  vVboPtr      = comp.OpVariable(fn, _ptr_Priate_uint, spv::StorageClassPrivate);
  vVertCount   = comp.OpVariable(fn, _ptr_Priate_uint, spv::StorageClassPrivate);
  if(emode==spv::ExecutionModelGLCompute) {
    vTmp = comp.OpVariable(fn, _ptr_Workgroup_uint, spv::StorageClassWorkgroup);
    }

  fn = comp.findSectionEnd(libspirv::Bytecode::S_Annotations);
  fn.insert(spv::OpDecorate,       {_runtimearr_uint, spv::DecorationArrayStride, 4});
  fn.insert(spv::OpDecorate,       {_runtimearr_desc, spv::DecorationArrayStride, 12});

  // fn.insert(spv::OpMemberDecorate, {IndirectCommand, 0, spv::DecorationOffset, 0});

  fn.insert(spv::OpMemberDecorate, {Descriptor, 0, spv::DecorationOffset, 0});
  fn.insert(spv::OpMemberDecorate, {Descriptor, 1, spv::DecorationOffset, 4});
  fn.insert(spv::OpMemberDecorate, {Descriptor, 2, spv::DecorationOffset, 8});

  fn.insert(spv::OpMemberDecorate, {DrawIndexedIndirectCommand, 0, spv::DecorationOffset, 0});
  fn.insert(spv::OpMemberDecorate, {DrawIndexedIndirectCommand, 1, spv::DecorationOffset, 4});
  fn.insert(spv::OpMemberDecorate, {DrawIndexedIndirectCommand, 2, spv::DecorationOffset, 8});
  fn.insert(spv::OpMemberDecorate, {DrawIndexedIndirectCommand, 3, spv::DecorationOffset, 12});
  fn.insert(spv::OpMemberDecorate, {DrawIndexedIndirectCommand, 4, spv::DecorationOffset, 16});
  fn.insert(spv::OpMemberDecorate, {DrawIndexedIndirectCommand, 5, spv::DecorationOffset, 20});
  fn.insert(spv::OpMemberDecorate, {DrawIndexedIndirectCommand, 6, spv::DecorationOffset, 24});
  fn.insert(spv::OpMemberDecorate, {DrawIndexedIndirectCommand, 7, spv::DecorationOffset, 28});

  fn.insert(spv::OpDecorate,       {vIndirectCmd,         spv::DecorationDescriptorSet, 1});
  fn.insert(spv::OpDecorate,       {vIndirectCmd,         spv::DecorationBinding, 0});
  fn.insert(spv::OpDecorate,       {EngineInternal0_t,    spv::DecorationBlock});
  fn.insert(spv::OpMemberDecorate, {EngineInternal0_t, 0, spv::DecorationOffset, 0});

  fn.insert(spv::OpDecorate,       {vDecriptors,          spv::DecorationDescriptorSet, 1});
  fn.insert(spv::OpDecorate,       {vDecriptors,          spv::DecorationBinding, 1});
  fn.insert(spv::OpDecorate,       {EngineInternal1_t,    spv::DecorationBlock});
  fn.insert(spv::OpMemberDecorate, {EngineInternal1_t, 0, spv::DecorationOffset,  0});
  fn.insert(spv::OpMemberDecorate, {EngineInternal1_t, 1, spv::DecorationOffset,  4});
  fn.insert(spv::OpMemberDecorate, {EngineInternal1_t, 2, spv::DecorationOffset,  8});
  fn.insert(spv::OpMemberDecorate, {EngineInternal1_t, 3, spv::DecorationOffset, 12});

  fn.insert(spv::OpDecorate,       {vScratch,             spv::DecorationDescriptorSet, 1});
  fn.insert(spv::OpDecorate,       {vScratch,             spv::DecorationBinding, 2});
  fn.insert(spv::OpDecorate,       {EngineInternal2_t,    spv::DecorationBlock});
  fn.insert(spv::OpMemberDecorate, {EngineInternal2_t, 0, spv::DecorationOffset, 0});
  fn.insert(spv::OpMemberDecorate, {EngineInternal2_t, 1, spv::DecorationOffset, 4});
  // fn.insert(spv::OpMemberDecorate, {EngineInternal2_t, 1, spv::DecorationCoherent});

  fn = comp.findSectionEnd(libspirv::Bytecode::S_Debug);
  //fn.insert(spv::OpName,       IndirectCommand,    "IndirectCommand");
  //fn.insert(spv::OpMemberName, IndirectCommand, 0, "indexCount");

  fn.insert(spv::OpName,       Descriptor,    "Descriptor");
  fn.insert(spv::OpMemberName, Descriptor, 0, "drawId");
  fn.insert(spv::OpMemberName, Descriptor, 1, "ptr");
  if(code.findExecutionModel()==spv::ExecutionModelMeshEXT) {
    fn.insert(spv::OpMemberName, Descriptor, 2, "indSz");
    } else {
    fn.insert(spv::OpMemberName, Descriptor, 2, "dispatchSz");
    }

  fn.insert(spv::OpName,       DrawIndexedIndirectCommand,    "DrawIndexedIndirectCommand");
  fn.insert(spv::OpMemberName, DrawIndexedIndirectCommand, 0, "drawId");
  if(code.findExecutionModel()==spv::ExecutionModelMeshEXT) {
    fn.insert(spv::OpMemberName, DrawIndexedIndirectCommand, 1, "indexCountSrc");
    fn.insert(spv::OpMemberName, DrawIndexedIndirectCommand, 2, "indexCount");
    fn.insert(spv::OpMemberName, DrawIndexedIndirectCommand, 3, "instanceCount");
    fn.insert(spv::OpMemberName, DrawIndexedIndirectCommand, 4, "firstIndex");
    fn.insert(spv::OpMemberName, DrawIndexedIndirectCommand, 5, "vertexOffset");
    fn.insert(spv::OpMemberName, DrawIndexedIndirectCommand, 6, "firstInstance");
    } else {
    fn.insert(spv::OpMemberName, DrawIndexedIndirectCommand, 1, "meshCountSrc");
    fn.insert(spv::OpMemberName, DrawIndexedIndirectCommand, 2, "dispatchX");
    fn.insert(spv::OpMemberName, DrawIndexedIndirectCommand, 3, "dispatchY");
    fn.insert(spv::OpMemberName, DrawIndexedIndirectCommand, 4, "dispatchZ");
    fn.insert(spv::OpMemberName, DrawIndexedIndirectCommand, 5, "vertexOffset");
    fn.insert(spv::OpMemberName, DrawIndexedIndirectCommand, 6, "taskCount");
    }
  fn.insert(spv::OpMemberName, DrawIndexedIndirectCommand, 7, "lutPtr");

  fn.insert(spv::OpName,       EngineInternal0_t,    "EngineInternal0");
  fn.insert(spv::OpMemberName, EngineInternal0_t, 0, "indirect");

  fn.insert(spv::OpName,       EngineInternal1_t,    "EngineInternal1");
  fn.insert(spv::OpMemberName, EngineInternal1_t, 0, "taskletCnt");
  fn.insert(spv::OpMemberName, EngineInternal1_t, 1, "meshletCnt");
  fn.insert(spv::OpMemberName, EngineInternal1_t, 2, "iterator");
  fn.insert(spv::OpMemberName, EngineInternal1_t, 3, "desc");

  fn.insert(spv::OpName,       EngineInternal2_t,    "EngineInternal2");
  fn.insert(spv::OpMemberName, EngineInternal2_t, 0, "grow");
  fn.insert(spv::OpMemberName, EngineInternal2_t, 1, "var");

  fn.insert(spv::OpName,       vIboPtr,    "iboPtr");
  fn.insert(spv::OpName,       vVboPtr,    "vboPtr");
  fn.insert(spv::OpName,       vVertCount, "maxVertex");
  if(emode==spv::ExecutionModelGLCompute) {
    fn.insert(spv::OpName, vTmp, "wgHelper");
    }

  fn = comp.findOpEntryPoint(emode,"main");
  assert(fn!=comp.end());
  fn.append(vIndirectCmd);
  fn.append(vDecriptors);
  fn.append(vScratch);
  fn.append(vIboPtr);
  fn.append(vVboPtr);
  fn.append(vVertCount);
  if(emode==spv::ExecutionModelGLCompute) {
    fn.append(vTmp);
    }
  }

void MeshConverter::emitEmitMeshTasks(libspirv::MutableBytecode& comp, uint32_t engSetMesh, uint32_t gl_LocalInvocationIndex) {
  auto fn = comp.findSectionEnd(libspirv::Bytecode::S_Debug);
  fn.insert(spv::OpName, engSetMesh, "OpEmitMeshTasks");

  // Types
  fn = comp.findSectionEnd(libspirv::Bytecode::S_Types);
  const uint32_t void_t               = comp.OpTypeVoid(fn);
  const uint32_t bool_t               = comp.OpTypeBool(fn);
  const uint32_t uint_t               = comp.OpTypeInt(fn, 32, false);
  const uint32_t _ptr_Storage_uint    = comp.OpTypePointer(fn, spv::StorageClassStorageBuffer, uint_t);
  const uint32_t _ptr_Workgroup_uint  = comp.OpTypePointer(fn, spv::StorageClassWorkgroup, uint_t);
  const uint32_t _ptr_Function_uint   = comp.OpTypePointer(fn, spv::StorageClassFunction, uint_t);
  const uint32_t func_void_uuu        = comp.OpTypeFunction(fn,void_t,{_ptr_Function_uint, _ptr_Function_uint, _ptr_Function_uint});

  code.traverseType(taskPayloadType,[&](const libspirv::MutableBytecode::AccessChain* ids, uint32_t len) {
    if(!code.isBasicTypeDecl(ids[len-1].type->op()))
      return;
    auto tid = (*ids[len-1].type)[1];
    comp.OpTypePointer(fn, spv::StorageClassWorkgroup, tid);
    });

  const uint32_t plSize    = typeSizeOf(taskPayloadType)/sizeof(uint32_t);
  const uint32_t const0    = constants[0];
  const uint32_t const1    = constants[1];
  const uint32_t const2    = constants[2];
  const uint32_t allocSize = comp.OpConstant(fn, uint_t, plSize + 3);
  const uint32_t const264  = comp.OpConstant(fn, uint_t, 264);

  // Function
  fn = comp.end();
  fn.insert(spv::OpFunction, {void_t, engSetMesh, spv::FunctionControlMaskNone, func_void_uuu});
  const uint32_t szX = comp.OpFunctionParameter(fn, _ptr_Function_uint);
  const uint32_t szY = comp.OpFunctionParameter(fn, _ptr_Function_uint);
  const uint32_t szZ = comp.OpFunctionParameter(fn, _ptr_Function_uint);

  fn.insert(spv::OpLabel, {comp.fetchAddBound()});

  fn.insert(spv::OpMemoryBarrier,  {constants[1], const264});               // memoryBarrierShared() // needed?
  fn.insert(spv::OpControlBarrier, {constants[2], constants[2], const264}); // barrier()

  const uint32_t rgThreadId = comp.fetchAddBound();
  fn.insert(spv::OpLoad, {uint_t, rgThreadId, gl_LocalInvocationIndex});

  // gl_LocalInvocationIndex!=0
  {
    const uint32_t cond0 = comp.fetchAddBound();
    fn.insert(spv::OpINotEqual, {bool_t, cond0, rgThreadId, const0});

    const uint32_t condBlockBegin = comp.fetchAddBound();
    const uint32_t condBlockEnd   = comp.fetchAddBound();
    fn.insert(spv::OpSelectionMerge,    {condBlockEnd, spv::SelectionControlMaskNone});
    fn.insert(spv::OpBranchConditional, {cond0, condBlockBegin, condBlockEnd});
    fn.insert(spv::OpLabel,             {condBlockBegin});
    fn.insert(spv::OpReturn,            {});
    //fn.insert(spv::OpBranch, {condBlockEnd});
    fn.insert(spv::OpLabel,  {condBlockEnd});
  }

  // dispatch_size ==0
  {
    const uint32_t rgX = comp.fetchAddBound();
    const uint32_t rgY = comp.fetchAddBound();
    const uint32_t rgZ = comp.fetchAddBound();

    fn.insert(spv::OpLoad, {uint_t, rgX, szX});
    fn.insert(spv::OpLoad, {uint_t, rgY, szY});
    fn.insert(spv::OpLoad, {uint_t, rgZ, szZ});

    const uint32_t cond0 = comp.fetchAddBound();
    const uint32_t cond1 = comp.fetchAddBound();
    const uint32_t cond2 = comp.fetchAddBound();
    fn.insert(spv::OpIEqual, {bool_t, cond0, rgX, const0});
    fn.insert(spv::OpIEqual, {bool_t, cond1, rgY, const0});
    fn.insert(spv::OpIEqual, {bool_t, cond2, rgZ, const0});

    const uint32_t rTmp0 = comp.fetchAddBound();
    fn.insert(spv::OpLogicalOr, {bool_t, rTmp0, cond0, cond1});
    const uint32_t rTmp1 = comp.fetchAddBound();
    fn.insert(spv::OpLogicalOr, {bool_t, rTmp1, rTmp0, cond2});

    const uint32_t condBlockBegin = comp.fetchAddBound();
    const uint32_t condBlockEnd   = comp.fetchAddBound();
    fn.insert(spv::OpSelectionMerge,    {condBlockEnd, spv::SelectionControlMaskNone});
    fn.insert(spv::OpBranchConditional, {rTmp1, condBlockBegin, condBlockEnd});
    fn.insert(spv::OpLabel,             {condBlockBegin});
    fn.insert(spv::OpReturn,            {});
    //fn.insert(spv::OpBranch, {condBlockEnd});
    fn.insert(spv::OpLabel,  {condBlockEnd});
  }

  const uint32_t ptrVarDest = comp.fetchAddBound();
  fn.insert(spv::OpAccessChain, {_ptr_Storage_uint, ptrVarDest, vScratch, mGrow}); //&EngineInternal2::grow

  const uint32_t rgTmp0 = comp.fetchAddBound();
  fn.insert(spv::OpAtomicIAdd, {uint_t, rgTmp0, ptrVarDest, const1/*scope*/, const0/*semantices*/, allocSize});
  fn.insert(spv::OpStore, {vTmp, rgTmp0});

  uint32_t seq        = 0;
  uint32_t dispatchSz = comp.fetchAddBound();
  // dispatch size
  const uint32_t szArr[] = {szX, szY, szZ};
  for(int i=0; i<3; ++i) {
    const uint32_t szV    = szArr[i];
    const uint32_t rgX    = comp.fetchAddBound();
    const uint32_t wgXptr = comp.fetchAddBound();
    const uint32_t rDst   = comp.fetchAddBound();
    fn.insert(spv::OpIAdd,        {uint_t, rDst, rgTmp0, constants[seq]});
    fn.insert(spv::OpLoad,        {uint_t, rgX, szV});
    fn.insert(spv::OpAccessChain, {_ptr_Storage_uint, wgXptr, vScratch, mVar, rDst});
    fn.insert(spv::OpStore,       {wgXptr, rgX});
    seq++;
    if(i==0) {
      dispatchSz = rgX;
      } else {
      uint32_t mulR = comp.fetchAddBound();
      fn.insert(spv::OpIMul, {uint_t, mulR, dispatchSz, rgX});
      dispatchSz = mulR;
      }
    }

  // taskPayload
  code.traverseType(taskPayloadType,[&](const libspirv::MutableBytecode::AccessChain* ids, uint32_t len) {
    if(!code.isBasicTypeDecl(ids[len-1].type->op()))
      return;

   // at += memberId
    const uint32_t rDst = comp.fetchAddBound();
    fn.insert(spv::OpIAdd, {uint_t, rDst, rgTmp0, constants[seq]});
    ++seq;
    // &EngineInternal2.var[dst]
    const uint32_t ptrHeap = comp.fetchAddBound();
    fn.insert(spv::OpAccessChain, {_ptr_Storage_uint, ptrHeap, vScratch, mVar, rDst});

    const uint32_t varPtr = comp.fetchAddBound();
    uint32_t chain[255] = {_ptr_Workgroup_uint, varPtr, taskPayload};
    uint32_t chainSz    = 3;
    for(uint32_t i=1; i+1<len; ++i) {
      chain[chainSz] = constants[ids[i].index];
      ++chainSz;
      }
    auto tid = (*ids[len-1].type)[1];
    chain[0] = comp.OpTypePointer(fn, spv::StorageClassWorkgroup, tid);
    fn.insert(spv::OpAccessChain, chain, chainSz);

    const uint32_t valP = comp.fetchAddBound();
    fn.insert(spv::OpLoad, {tid, valP, varPtr});
    if(tid!=uint_t) {
      const uint32_t rCast = comp.fetchAddBound();
      fn.insert(spv::OpBitcast, {uint_t, rCast, valP});
      fn.insert(spv::OpStore,   {ptrHeap, rCast});
      } else {
      fn.insert(spv::OpStore,   {ptrHeap, valP});
      }
    });

  // drawId
  const uint32_t drawId   = comp.fetchAddBound();
  const uint32_t rgDrawId = comp.fetchAddBound();
  fn.insert(spv::OpAccessChain, {_ptr_Storage_uint, drawId, vIndirectCmd, const0, const0}); //&EngineInternal0::indirect.drawId
  fn.insert(spv::OpLoad, {uint_t, rgDrawId, drawId});

  // vIndirectCmd[i].indexCountSrc += N;
  const uint32_t ptrCmdDest = comp.fetchAddBound();
  const uint32_t rgTmp1     = comp.fetchAddBound();
  fn.insert(spv::OpAccessChain, {_ptr_Storage_uint, ptrCmdDest, vIndirectCmd, const0, const1}); //&EngineInternal0::indirect.indexCountSrc
  fn.insert(spv::OpAtomicIAdd, {uint_t, rgTmp1, ptrCmdDest, const1/*scope*/, const0/*semantices*/, dispatchSz});

  // vDecriptors
  const uint32_t ptrDescDest = comp.fetchAddBound();
  fn.insert(spv::OpAccessChain, {_ptr_Storage_uint, ptrDescDest, vDecriptors, mTaskC}); //&EngineInternal1::taskletCnt
  const uint32_t rgTmp2 = comp.fetchAddBound();
  fn.insert(spv::OpAtomicIAdd, {uint_t, rgTmp2, ptrDescDest, const1/*scope*/, const0/*semantices*/, const1});

  const uint32_t descDestDr = comp.fetchAddBound();
  fn.insert(spv::OpAccessChain, {_ptr_Storage_uint, descDestDr,  vDecriptors, mDesc, rgTmp2, const0}); //&EngineInternal1::desc[].drawId
  fn.insert(spv::OpStore, {descDestDr, rgDrawId});
  const uint32_t descDestInd = comp.fetchAddBound();
  fn.insert(spv::OpAccessChain, {_ptr_Storage_uint, descDestInd, vDecriptors, mDesc, rgTmp2, const1}); //&EngineInternal1::desc[].ptr
  fn.insert(spv::OpStore, {descDestInd, rgTmp0});
  const uint32_t descDestSz = comp.fetchAddBound();
  fn.insert(spv::OpAccessChain, {_ptr_Storage_uint, descDestSz,  vDecriptors, mDesc, rgTmp2, const2}); //&EngineInternal1::desc[].dispatchSz
  fn.insert(spv::OpStore, {descDestSz, dispatchSz});

  fn.insert(spv::OpReturn,      {});
  fn.insert(spv::OpFunctionEnd, {});
  }

void MeshConverter::emitSetMeshOutputs(libspirv::MutableBytecode& comp, uint32_t engSetMesh,
                                       uint32_t gl_LocalInvocationIndex, uint32_t gl_WorkGroupID) {
  auto fn = comp.findSectionEnd(libspirv::Bytecode::S_Debug);
  fn.insert(spv::OpName, engSetMesh, "OpSetMeshOutputs");

  // Types
  fn = comp.findSectionEnd(libspirv::Bytecode::S_Types);
  const uint32_t void_t              = comp.OpTypeVoid(fn);
  const uint32_t bool_t              = comp.OpTypeBool(fn);
  const uint32_t uint_t              = comp.OpTypeInt(fn, 32,false);
  const uint32_t _ptr_Storage_uint   = comp.OpTypePointer(fn, spv::StorageClassStorageBuffer, uint_t);
  const uint32_t _ptr_Function_uint  = comp.OpTypePointer(fn, spv::StorageClassFunction,      uint_t);
  const uint32_t _ptr_Input_uint     = comp.OpTypePointer(fn, spv::StorageClassInput,         uint_t);

  const uint32_t func_void_uu        = comp.OpTypeFunction(fn,void_t,{_ptr_Function_uint, _ptr_Function_uint});

  // Constants
  const uint32_t const0              = comp.OpConstant(fn,uint_t,0);
  const uint32_t const1              = comp.OpConstant(fn,uint_t,1);
  const uint32_t const2              = comp.OpConstant(fn,uint_t,2);
  const uint32_t const3              = comp.OpConstant(fn,uint_t,3);
  const uint32_t constVertSz         = comp.OpConstant(fn,uint_t,varCount);
  const uint32_t const264            = comp.OpConstant(fn,uint_t,264);

  // Function
  fn = comp.end();
  fn.insert(spv::OpFunction,         {void_t, engSetMesh, spv::FunctionControlMaskNone, func_void_uu});
  const uint32_t maxV = comp.OpFunctionParameter(fn, _ptr_Function_uint);
  const uint32_t maxP = comp.OpFunctionParameter(fn, _ptr_Function_uint);

  fn.insert(spv::OpLabel,            {comp.fetchAddBound()});

  const uint32_t rgAllocSize     = comp.fetchAddBound();
  const uint32_t rgIboSize       = comp.fetchAddBound();
  const uint32_t indPerPrimitive = const3; // Tringles
  if(options.deferredMeshShading) {
    const uint32_t rgMaxV = comp.fetchAddBound();
    const uint32_t rgMaxP = comp.fetchAddBound();
    fn.insert(spv::OpLoad, {uint_t, rgMaxV, maxV});
    fn.insert(spv::OpLoad, {uint_t, rgMaxP, maxP});

    const uint32_t rg0 = comp.fetchAddBound();
    fn.insert(spv::OpIMul,    {uint_t, rg0, rgMaxV, constVertSz});
    fn.insert(spv::OpIMul,    {uint_t, rgIboSize, rgMaxP, indPerPrimitive});
    fn.insert(spv::OpBitcast, {uint_t, rgAllocSize, rgIboSize});
    } else {
    const uint32_t rgMaxV = comp.fetchAddBound();
    const uint32_t rgMaxP = comp.fetchAddBound();
    fn.insert(spv::OpLoad, {uint_t, rgMaxV, maxV});
    fn.insert(spv::OpLoad, {uint_t, rgMaxP, maxP});

    const uint32_t rg0 = comp.fetchAddBound();
    fn.insert(spv::OpIMul, {uint_t, rg0, rgMaxV, constVertSz});
    fn.insert(spv::OpIMul, {uint_t, rgIboSize, rgMaxP, indPerPrimitive});
    fn.insert(spv::OpIAdd, {uint_t, rgAllocSize, rg0, rgIboSize});
    }

  const uint32_t rgThreadId    = comp.fetchAddBound();
  fn.insert(spv::OpLoad, {uint_t, rgThreadId, gl_LocalInvocationIndex});

  const uint32_t drawId   = comp.fetchAddBound();
  const uint32_t rgDrawId = comp.fetchAddBound();
  fn.insert(spv::OpAccessChain, {_ptr_Storage_uint, drawId, vIndirectCmd, const0, const0}); //&EngineInternal0::indirect.drawId
  fn.insert(spv::OpLoad, {uint_t, rgDrawId, drawId});

  // gl_LocalInvocationIndex==0
  {
    const uint32_t cond0 = comp.fetchAddBound();
    fn.insert(spv::OpIEqual, {bool_t, cond0, rgThreadId, const0});

    const uint32_t condBlockBegin = comp.fetchAddBound();
    const uint32_t condBlockEnd   = comp.fetchAddBound();
    fn.insert(spv::OpSelectionMerge,    {condBlockEnd, spv::SelectionControlMaskNone});
    fn.insert(spv::OpBranchConditional, {cond0, condBlockBegin, condBlockEnd});
    fn.insert(spv::OpLabel,             {condBlockBegin});
    // fn.insert(spv::OpReturn,            {});

    const uint32_t ptrVarDest = comp.fetchAddBound();
    fn.insert(spv::OpAccessChain, {_ptr_Storage_uint, ptrVarDest, vScratch, mGrow}); //&EngineInternal2::grow

    const uint32_t rgTmp0 = comp.fetchAddBound();
    fn.insert(spv::OpAtomicIAdd, {uint_t, rgTmp0, ptrVarDest, const1/*scope*/, const0/*semantices*/, rgAllocSize});

    fn.insert(spv::OpStore, {vTmp, rgTmp0});

    fn.insert(spv::OpBranch, {condBlockEnd});
    fn.insert(spv::OpLabel,  {condBlockEnd});
  }

  fn.insert(spv::OpMemoryBarrier,  {const1, const264});         // memoryBarrierShared() // needed?
  fn.insert(spv::OpControlBarrier, {const2, const2, const264}); // barrier()

  // iboPtr =
  // vboPtr =
  {
  const uint32_t rg0 = comp.fetchAddBound();
  fn.insert(spv::OpLoad, {uint_t, rg0, vTmp});
  fn.insert(spv::OpStore, {vIboPtr, rg0});

  if(options.deferredMeshShading) {
    const uint32_t wgSize = workGroupSize[0]*workGroupSize[1]*workGroupSize[2];
    fn = comp.findSectionEnd(libspirv::Bytecode::S_Types);
    const uint32_t uint_t = comp.OpTypeInt(fn, 32, false);
    const uint32_t constI = comp.OpConstant(fn,uint_t,wgSize);

    fn = comp.end();
    const uint32_t rg1 = comp.fetchAddBound();
    fn.insert(spv::OpAccessChain, {_ptr_Input_uint, rg1, gl_WorkGroupID, const0});
    const uint32_t rg2 = comp.fetchAddBound();
    fn.insert(spv::OpLoad,  {uint_t, rg2, rg1});

    const uint32_t rg3 = comp.fetchAddBound();
    fn.insert(spv::OpIMul,  {uint_t, rg3, rg2, constI});

    fn.insert(spv::OpStore, {vVboPtr, rg3});
    } else {
    const uint32_t rgMaxP = comp.fetchAddBound();
    fn.insert(spv::OpLoad, {uint_t, rgMaxP, maxP});

    const uint32_t rg1 = comp.fetchAddBound();
    fn.insert(spv::OpIMul, {uint_t, rg1, rgMaxP, const3}); // Tringles
    const uint32_t rg2 = comp.fetchAddBound();
    fn.insert(spv::OpIAdd, {uint_t, rg2, rg0, rg1});

    fn.insert(spv::OpStore, {vVboPtr, rg2});
    }
  }

  // maxVertex =
  {
    const uint32_t rgMaxV = comp.fetchAddBound();
    fn.insert(spv::OpLoad, {uint_t, rgMaxV, maxV});
    fn.insert(spv::OpStore, {vVertCount, rgMaxV});
  }

  // gl_LocalInvocationIndex==0
  {
    const uint32_t cond0 = comp.fetchAddBound();
    fn.insert(spv::OpIEqual, {bool_t, cond0, rgThreadId, const0});

    const uint32_t condBlockBegin = comp.fetchAddBound();
    const uint32_t condBlockEnd   = comp.fetchAddBound();
    fn.insert(spv::OpSelectionMerge,    {condBlockEnd, spv::SelectionControlMaskNone});
    fn.insert(spv::OpBranchConditional, {cond0, condBlockBegin, condBlockEnd});
    fn.insert(spv::OpLabel,             {condBlockBegin});
    // fn.insert(spv::OpReturn,            {});

    const uint32_t rgTmp0 = comp.fetchAddBound();
    fn.insert(spv::OpLoad, {uint_t, rgTmp0, vTmp});

    // vIndirectCmd[i].indexCountSrc += N;
    const uint32_t ptrCmdDest = comp.fetchAddBound();
    const uint32_t rgTmp1     = comp.fetchAddBound();
    fn.insert(spv::OpAccessChain, {_ptr_Storage_uint, ptrCmdDest, vIndirectCmd, const0, const1}); //&EngineInternal0::indirect.indexCountSrc
    fn.insert(spv::OpAtomicIAdd, {uint_t, rgTmp1, ptrCmdDest, const1/*scope*/, const0/*semantices*/, rgIboSize});

    // vDecriptors
    const uint32_t ptrDescDest = comp.fetchAddBound();
    fn.insert(spv::OpAccessChain, {_ptr_Storage_uint, ptrDescDest, vDecriptors, mMeshC}); //&EngineInternal1::meshletCnt
    const uint32_t rgTmp2 = comp.fetchAddBound();
    fn.insert(spv::OpAtomicIAdd, {uint_t, rgTmp2, ptrDescDest, const1/*scope*/, const0/*semantices*/, const1});

    const uint32_t descDestDr = comp.fetchAddBound();
    fn.insert(spv::OpAccessChain, {_ptr_Storage_uint, descDestDr,  vDecriptors, mDesc, rgTmp2, const0}); //&EngineInternal1::desc[].drawId
    fn.insert(spv::OpStore, {descDestDr, rgDrawId});
    const uint32_t descDestInd = comp.fetchAddBound();
    fn.insert(spv::OpAccessChain, {_ptr_Storage_uint, descDestInd, vDecriptors, mDesc, rgTmp2, const1}); //&EngineInternal1::desc[].ptr
    fn.insert(spv::OpStore, {descDestInd, rgTmp0});
    const uint32_t descDestSz = comp.fetchAddBound();
    fn.insert(spv::OpAccessChain, {_ptr_Storage_uint, descDestSz,  vDecriptors, mDesc, rgTmp2, const2}); //&EngineInternal1::desc[].indSz
    fn.insert(spv::OpStore, {descDestSz, rgIboSize});

    fn.insert(spv::OpBranch, {condBlockEnd});
    fn.insert(spv::OpLabel,  {condBlockEnd});
  }

  fn.insert(spv::OpReturn,           {});
  fn.insert(spv::OpFunctionEnd,      {});
  }

void MeshConverter::emitTailStore(libspirv::MutableBytecode& comp, uint32_t engTail, uint32_t gl_LocalInvocationIndex) {
  auto fn = comp.findSectionEnd(libspirv::Bytecode::S_Debug);
  fn.insert(spv::OpName, engTail, "_FlushVaryings");

  const uint32_t wgSize = workGroupSize[0]*workGroupSize[1]*workGroupSize[2];

  // Types
  fn = comp.findSectionEnd(libspirv::Bytecode::S_Types);
  const uint32_t void_t    = comp.OpTypeVoid(fn);
  const uint32_t uint_t    = comp.OpTypeInt(fn,32,false);
  const uint32_t float_t   = comp.OpTypeFloat(fn,32);
  const uint32_t func_void = comp.OpTypeFunction(fn,void_t,{});

  const uint32_t _ptr_Function_uint   = comp.OpTypePointer(fn, spv::StorageClassFunction, uint_t);
  const uint32_t _ptr_Storage_uint    = comp.OpTypePointer(fn, spv::StorageClassStorageBuffer, uint_t);

  const uint32_t _ptr_Workgroup_uint  = comp.OpTypePointer(fn, spv::StorageClassWorkgroup, uint_t);
  const uint32_t _ptr_Workgroup_float = comp.OpTypePointer(fn, spv::StorageClassWorkgroup, float_t);

  // Constants
  const uint32_t constWgSize = comp.OpConstant(fn, uint_t, wgSize);
  const uint32_t constVertSz = constants[varCount];

  // Function
  fn = comp.end();
  fn.insert(spv::OpFunction, {void_t, engTail, spv::FunctionControlMaskNone, func_void});
  fn.insert(spv::OpLabel,    {comp.fetchAddBound()});

  // Var
  auto varI = comp.OpVariable(fn, _ptr_Function_uint, spv::StorageClassFunction);

  const uint32_t maxVertex = comp.fetchAddBound();
  fn.insert(spv::OpLoad,  {uint_t, maxVertex, vVertCount});

  const uint32_t rgThreadId = comp.fetchAddBound();
  fn.insert(spv::OpLoad, {uint_t, rgThreadId, gl_LocalInvocationIndex});

  fn = emitLoop(comp, varI, rgThreadId, maxVertex, constWgSize, [&](libspirv::MutableBytecode::Iterator& fn) {
    uint32_t seq = 0;
    const uint32_t rI  = comp.fetchAddBound();
    fn.insert(spv::OpLoad, {uint_t, rI,  varI});

    const uint32_t rAt = comp.fetchAddBound();
    const uint32_t regPVboBase = comp.fetchAddBound();
    fn.insert(spv::OpLoad, {uint_t, regPVboBase, vVboPtr});

    auto rg0 = comp.fetchAddBound();
    fn.insert(spv::OpIMul, {uint_t, rg0, constVertSz, rI});
    fn.insert(spv::OpIAdd, {uint_t, rAt, regPVboBase, rg0});

    for(auto& i:varying) {
      uint32_t type = i.second.type;

      code.traverseType(type,[&](const libspirv::MutableBytecode::AccessChain* ids, uint32_t len) {
        if(!code.isBasicTypeDecl(ids[len-1].type->op()))
          return;
        if(len<2 || ids[1].index!=0)
          return; // [max_vertex] arrayness

        // at += memberId
        const uint32_t rDst = comp.fetchAddBound();
        fn.insert(spv::OpIAdd, {uint_t, rDst, rAt, constants[seq]});
        ++seq;

        // &EngineInternal2.var[dst]
        const uint32_t ptrHeap = comp.fetchAddBound();
        fn.insert(spv::OpAccessChain, {_ptr_Storage_uint, ptrHeap, vScratch, mVar, rDst});

        // NOTE: ids is pointer to array of X, we need only X
        const uint32_t varPtr = comp.fetchAddBound();
        uint32_t chain[255] = {_ptr_Workgroup_float, varPtr, i.first, rI};
        uint32_t chainSz    = 4;
        for(uint32_t i=2; i+1<len; ++i) {
          chain[chainSz] = constants[ids[i].index];
          ++chainSz;
          }
        fn.insert(spv::OpAccessChain, chain, chainSz);

        const uint32_t valR = comp.fetchAddBound();
        if(ids[len-1].type->op()==spv::OpTypeFloat) {
          const uint32_t rCast = comp.fetchAddBound();
          chain[0] = _ptr_Workgroup_float;
          fn.insert(spv::OpLoad,    {float_t, valR,  varPtr});
          fn.insert(spv::OpBitcast, {uint_t,  rCast, valR});
          fn.insert(spv::OpStore,   {ptrHeap, rCast});
          } else {
          chain[0] = _ptr_Workgroup_uint;
          fn.insert(spv::OpLoad,  {uint_t,  valR, varPtr});
          fn.insert(spv::OpStore, {ptrHeap, valR});
          }
        });
      }
    });

  fn.insert(spv::OpReturn,      {});
  fn.insert(spv::OpFunctionEnd, {});
  }

void MeshConverter::emitPayoadLoad(libspirv::MutableBytecode& comp, uint32_t engHead) {
  auto fn = comp.findSectionEnd(libspirv::Bytecode::S_Debug);
  fn.insert(spv::OpName, engHead, "_PayloadLoad");

  // Types
  fn = comp.findSectionEnd(libspirv::Bytecode::S_Types);
  const uint32_t void_t               = comp.OpTypeVoid(fn);
  const uint32_t uint_t               = comp.OpTypeInt(fn,32,false);
  const uint32_t bool_t               = comp.OpTypeBool(fn);
  const uint32_t uvec3_t              = comp.OpTypeVector(fn,uint_t,3);
  const uint32_t func_void            = comp.OpTypeFunction(fn,void_t,{});
  const uint32_t _ptr_Storage_uint    = comp.OpTypePointer(fn, spv::StorageClassStorageBuffer, uint_t);
  const uint32_t _ptr_Workgroup_uint  = comp.OpTypePointer(fn, spv::StorageClassWorkgroup, uint_t);
  const uint32_t _ptr_Input_uint      = comp.OpTypePointer(fn, spv::StorageClassInput, uint_t);
  const uint32_t _ptr_Private_uint    = comp.OpTypePointer(fn, spv::StorageClassPrivate, uint_t);

  code.traverseType(taskPayloadType,[&](const libspirv::MutableBytecode::AccessChain* ids, uint32_t len) {
    if(!code.isBasicTypeDecl(ids[len-1].type->op()))
      return;
    auto tid = (*ids[len-1].type)[1];
    comp.OpTypePointer(fn, spv::StorageClassWorkgroup, tid);
    });

  // Constants
  const uint32_t const0     = constants[0];
  const uint32_t const7     = constants[7];
  const uint32_t const20    = comp.OpConstant(fn, uint_t, 20);
  const uint32_t constFFFFF = comp.OpConstant(fn, uint_t, 0xFFFFF);

  // Function
  fn = comp.end();
  fn.insert(spv::OpFunction, {void_t, engHead, spv::FunctionControlMaskNone, func_void});
  fn.insert(spv::OpLabel,    {comp.fetchAddBound()});

  if(usr_WorkGroupID!=0) {
    uint32_t rWg = comp.fetchAddBound();
    fn.insert(spv::OpLoad,  {uvec3_t, rWg, gl_WorkGroupID});
    fn.insert(spv::OpStore, {usr_WorkGroupID, rWg});
    }

  // lutPtr
  const uint32_t baseLutPtr   = comp.fetchAddBound();
  const uint32_t rgBaseLutPtr = comp.fetchAddBound();
  fn.insert(spv::OpAccessChain, {_ptr_Storage_uint, baseLutPtr, vIndirectCmd, const0, const7}); //&EngineInternal0::indirect.lutPtr
  fn.insert(spv::OpLoad, {uint_t, rgBaseLutPtr, baseLutPtr});
  // lutPtr==0
  {
    const uint32_t cond0 = comp.fetchAddBound();
    fn.insert(spv::OpIEqual, {bool_t, cond0, rgBaseLutPtr, const0});

    const uint32_t condBlockBegin = comp.fetchAddBound();
    const uint32_t condBlockEnd   = comp.fetchAddBound();
    fn.insert(spv::OpSelectionMerge,    {condBlockEnd, spv::SelectionControlMaskNone});
    fn.insert(spv::OpBranchConditional, {cond0, condBlockBegin, condBlockEnd});
    fn.insert(spv::OpLabel,             {condBlockBegin});
    fn.insert(spv::OpReturn,            {});
    //fn.insert(spv::OpBranch, {condBlockEnd});
    fn.insert(spv::OpLabel,  {condBlockEnd});
  }

  // &EngineInternal2.var[lutPtr + gl_WorkGroupID]
  uint32_t firstTask_lutPtr = comp.fetchAddBound();
  {
    const uint32_t rLut    = comp.fetchAddBound();
    const uint32_t ptrWgId = comp.fetchAddBound();
    fn.insert(spv::OpAccessChain, {_ptr_Input_uint, ptrWgId, gl_WorkGroupID, const0});
    uint32_t rTmp0 = comp.fetchAddBound();
    fn.insert(spv::OpLoad,  {uint_t, rTmp0, ptrWgId});

    fn.insert(spv::OpIAdd, {uint_t, rLut, rTmp0, rgBaseLutPtr});

    const uint32_t ptrHeap = comp.fetchAddBound();
    fn.insert(spv::OpAccessChain, {_ptr_Storage_uint, ptrHeap, vScratch, mVar, rLut});
    fn.insert(spv::OpLoad, {uint_t, firstTask_lutPtr, ptrHeap});
  }

  uint32_t rAt = comp.fetchAddBound();
  {
    // firstTask_lutPtr
    fn.insert(spv::OpBitwiseAnd, {uint_t, rAt, firstTask_lutPtr, constFFFFF});
  }

  if(usr_WorkGroupID!=0) {
    // firstTask_lutPtr
    uint32_t rWg0 = comp.fetchAddBound();
    fn.insert(spv::OpShiftRightLogical, {uint_t, rWg0, firstTask_lutPtr, const20});

    const uint32_t ptrWgId = comp.fetchAddBound();
    fn.insert(spv::OpAccessChain, {_ptr_Input_uint, ptrWgId, gl_WorkGroupID, const0});

    // dispatchX/Y/Z
    uint32_t rDispatch[3] = {};
    uint32_t ptrUWgId[3] = {};
    for(int i=0; i<3; ++i) {
      ptrUWgId[i] = comp.fetchAddBound();
      fn.insert(spv::OpAccessChain, {_ptr_Private_uint, ptrUWgId[i], usr_WorkGroupID, constants[i]});

      const uint32_t rDst = comp.fetchAddBound();
      fn.insert(spv::OpIAdd, {uint_t, rDst, rAt, constants[i]});
      const uint32_t ptrDispatchX = comp.fetchAddBound();
      fn.insert(spv::OpAccessChain, {_ptr_Storage_uint, ptrDispatchX, vScratch, mVar, rDst});
      rDispatch[i] = comp.fetchAddBound();
      fn.insert(spv::OpLoad, {uint_t, rDispatch[i], ptrDispatchX});
      }

    const uint32_t rTmp1 = comp.fetchAddBound();
    fn.insert(spv::OpUMod,  {uint_t, rTmp1, rWg0, rDispatch[0]});
    fn.insert(spv::OpStore, {ptrUWgId[0], rTmp1});

    const uint32_t rTmp2 = comp.fetchAddBound();
    fn.insert(spv::OpUDiv,  {uint_t, rTmp2, rWg0, rDispatch[0]});
    const uint32_t rTmp3 = comp.fetchAddBound();
    fn.insert(spv::OpUMod,  {uint_t, rTmp3, rTmp2, rDispatch[1]});
    fn.insert(spv::OpStore, {ptrUWgId[1], rTmp3});

    const uint32_t rTmp4 = comp.fetchAddBound();
    fn.insert(spv::OpUDiv,  {uint_t, rTmp4, rTmp2, rDispatch[1]});
    fn.insert(spv::OpStore, {ptrUWgId[2], rTmp4});
    }

  // taskPayload
  uint32_t seq = 3;
  code.traverseType(taskPayloadType,[&](const libspirv::MutableBytecode::AccessChain* ids, uint32_t len) {
    if(!code.isBasicTypeDecl(ids[len-1].type->op()))
      return;
    // at += memberId
    const uint32_t rDst = comp.fetchAddBound();
    assert(seq < constants.size());
    fn.insert(spv::OpIAdd, {uint_t, rDst, rAt, constants[seq]});
    ++seq;

    // &EngineInternal2.var[dst]
    const uint32_t ptrHeap = comp.fetchAddBound();
    fn.insert(spv::OpAccessChain, {_ptr_Storage_uint, ptrHeap, vScratch, mVar, rDst});

    const uint32_t varPtr = comp.fetchAddBound();
    uint32_t chain[255] = {_ptr_Workgroup_uint, varPtr, taskPayload};
    uint32_t chainSz    = 3;
    for(uint32_t i=1; i+1<len; ++i) {
      chain[chainSz] = constants[ids[i].index];
      ++chainSz;
      }
    auto tid = (*ids[len-1].type)[1];
    chain[0] = comp.OpTypePointer(fn, spv::StorageClassWorkgroup, tid);
    fn.insert(spv::OpAccessChain, chain, chainSz);
    //return;

    const uint32_t valP = comp.fetchAddBound();
    fn.insert(spv::OpLoad, {uint_t, valP, ptrHeap});

    if(tid!=uint_t) {
      const uint32_t rCast = comp.fetchAddBound();
      fn.insert(spv::OpBitcast, {tid, rCast, valP});
      fn.insert(spv::OpStore,   {varPtr, rCast});
      } else {
      fn.insert(spv::OpStore,   {varPtr, valP});
      }
    });

  fn.insert(spv::OpReturn,      {});
  fn.insert(spv::OpFunctionEnd, {});
  }

void MeshConverter::emitEngMain(libspirv::MutableBytecode& comp, uint32_t engMain, uint32_t headFn, uint32_t tailFn) {
  auto fn = comp.findSectionEnd(libspirv::Bytecode::S_Types);

  const uint32_t uint_t    = comp.OpTypeInt(fn, 32,false);
  const uint32_t const264  = comp.OpConstant(fn,uint_t,264);

  const uint32_t void_t    = comp.OpTypeVoid(fn);
  const uint32_t func_void = comp.OpTypeFunction(fn,void_t);

  fn = comp.end();
  fn.insert(spv::OpFunction, {void_t, engMain, spv::FunctionControlMaskNone, func_void});
  fn.insert(spv::OpLabel,    {comp.fetchAddBound()});

  if(/*taskPayload!=0 &&*/ headFn!=0) {
    const uint32_t tmp2 = comp.fetchAddBound();
    fn.insert(spv::OpFunctionCall, {void_t, tmp2, headFn});

    fn.insert(spv::OpMemoryBarrier,  {constants[1], const264});               // memoryBarrierShared() // needed?
    fn.insert(spv::OpControlBarrier, {constants[2], constants[2], const264}); // barrier()
    }

  const uint32_t tmp0 = comp.fetchAddBound();
  fn.insert(spv::OpFunctionCall, {void_t, tmp0, main});

  if(tailFn!=0) {
    fn.insert(spv::OpMemoryBarrier,  {constants[1], const264});               // memoryBarrierShared() // needed?
    fn.insert(spv::OpControlBarrier, {constants[2], constants[2], const264}); // barrier()

    const uint32_t tmp1 = comp.fetchAddBound();
    fn.insert(spv::OpFunctionCall, {void_t, tmp1, tailFn});
    }

  fn.insert(spv::OpReturn,      {});
  fn.insert(spv::OpFunctionEnd, {});
  }

void MeshConverter::emitVboStore(libspirv::MutableBytecode& comp, libspirv::MutableBytecode::Iterator& gen,
                                  const libspirv::Bytecode::OpCode& op, uint32_t achain) {
  if(options.deferredMeshShading)
    return;

  const auto val = op[2];

  // Types
  auto fn = comp.findSectionEnd(libspirv::Bytecode::S_Types);
  const uint32_t uint_t            = comp.OpTypeInt(fn, 32,false);
  const uint32_t _ptr_Storage_uint = comp.OpTypePointer(fn, spv::StorageClassStorageBuffer, uint_t);

  // Constants
  const uint32_t constVertSz       = constants[varCount];

  // Function
  const auto& chain = *code.fromOffset(achain);
  uint32_t clen = chain.length();

  uint32_t lchain[32] = {};
  std::memcpy(lchain+4, &chain[3], (clen-3)*sizeof(uint32_t));

  lchain[0] = _ptr_Storage_uint;
  // lchain[1] = comp.fetchAddBound();
  lchain[2] = vScratch;
  lchain[3] = mVar;

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
    uint32_t ix = chain[uint16_t(i)];

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

void MeshConverter::emitIboStore(libspirv::MutableBytecode& comp, libspirv::MutableBytecode::Iterator& gen,
                                  const libspirv::Bytecode::OpCode& op, uint32_t achain) {
  const auto triplet = op[2];

  // Types
  auto fn = comp.findSectionEnd(libspirv::Bytecode::S_Types);
  const uint32_t uint_t             = comp.OpTypeInt(fn, 32,false);
  const uint32_t _ptr_Storage_uint  = comp.OpTypePointer(fn, spv::StorageClassStorageBuffer, uint_t);

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
    nchain[2] = vScratch;
    nchain[3] = mVar;
    nchain[4] = regPIbo;
    gen.insert(spv::OpAccessChain, nchain, 5);

    auto elt = comp.fetchAddBound();
    gen.insert(spv::OpCompositeExtract, {uint_t, elt, triplet, uint32_t(i)});

    if(options.deferredMeshShading) {
      const uint32_t rg2 = comp.fetchAddBound();
      gen.insert(spv::OpLoad, {uint_t, rg2, vVboPtr});

      const uint32_t rg3 = comp.fetchAddBound();
      gen.insert(spv::OpIAdd, {uint_t, rg3, rg2, elt});
      gen.insert(spv::OpStore, {nchain[1], rg3});
      } else {
      const uint32_t rg1 = comp.fetchAddBound();
      gen.insert(spv::OpIMul, {uint_t, rg1, elt, constVertSz});

      const uint32_t rg2 = comp.fetchAddBound();
      gen.insert(spv::OpLoad, {uint_t, rg2, vVboPtr});

      const uint32_t rg3 = comp.fetchAddBound();
      gen.insert(spv::OpIAdd, {uint_t, rg3, rg2, rg1});
      gen.insert(spv::OpStore, {nchain[1], rg3});
      }
    }
  }

auto MeshConverter::emitLoop(libspirv::MutableBytecode& comp, uint32_t varI,
                             uint32_t begin, uint32_t end, uint32_t inc,
                             std::function<void (libspirv::MutableBytecode::Iterator&)> body) -> libspirv::MutableBytecode::Iterator{
  auto it = comp.findSectionEnd(libspirv::Bytecode::S_Types);
  const uint32_t tBool = comp.OpTypeBool(it);
  const uint32_t tUInt = comp.OpTypeInt (it,32,false);

  it = comp.end();
  it.insert(spv::OpStore, {varI, begin});
  const uint32_t loopBegin = comp.fetchAddBound();
  const uint32_t loopBody  = comp.fetchAddBound();
  const uint32_t loopEnd   = comp.fetchAddBound();
  const uint32_t loopStep  = comp.fetchAddBound();
  const uint32_t loopCond  = comp.fetchAddBound();
  const uint32_t rgCond    = comp.fetchAddBound();
  const uint32_t rgI0      = comp.fetchAddBound();
  const uint32_t rgI1      = comp.fetchAddBound();
  const uint32_t rgI2      = comp.fetchAddBound();

  it.insert(spv::OpBranch,            {loopBegin});
  it.insert(spv::OpLabel,             {loopBegin});

  it.insert(spv::OpLoopMerge,         {loopEnd, loopStep, spv::SelectionControlMaskNone});
  it.insert(spv::OpBranch,            {loopCond});

  it.insert(spv::OpLabel,             {loopCond});
  it.insert(spv::OpLoad,              {tUInt, rgI0, varI});
  it.insert(spv::OpULessThan,         {tBool, rgCond, rgI0, end});
  it.insert(spv::OpBranchConditional, {rgCond, loopBody, loopEnd});

  it.insert(spv::OpLabel,             {loopBody});
  body(it);
  it.insert(spv::OpBranch,            {loopStep});

  it.insert(spv::OpLabel,             {loopStep});
  it.insert(spv::OpLoad,              {tUInt, rgI1, varI});
  it.insert(spv::OpIAdd,              {tUInt, rgI2, rgI1, inc});
  it.insert(spv::OpStore,             {varI,  rgI2});
  it.insert(spv::OpBranch,            {loopBegin});

  it.insert(spv::OpLabel,             {loopEnd});

  return it;
  }

void MeshConverter::emitVert(libspirv::MutableBytecode& vert) {
  const uint32_t glslStd           = vert.fetchAddBound();
  const uint32_t main              = vert.fetchAddBound();
  const uint32_t lblMain           = vert.fetchAddBound();
  const uint32_t rg_VertexIndex    = vert.fetchAddBound();
  const uint32_t gl_VertexIndex    = vert.fetchAddBound();
  const uint32_t vScratch          = vert.fetchAddBound();

  const uint32_t void_t            = vert.fetchAddBound();
  const uint32_t bool_t            = vert.fetchAddBound();
  const uint32_t int_t             = vert.fetchAddBound();
  const uint32_t uint_t            = vert.fetchAddBound();
  const uint32_t float_t           = vert.fetchAddBound();
  const uint32_t _runtimearr_uint  = vert.fetchAddBound();
  const uint32_t func_t            = vert.fetchAddBound();

  const uint32_t _ptr_Input_int    = vert.fetchAddBound();
  const uint32_t _ptr_Output_uint  = vert.fetchAddBound();
  const uint32_t _ptr_Output_float = vert.fetchAddBound();
  const uint32_t _ptr_Storage_uint = vert.fetchAddBound();

  const uint32_t EngineInternal2_t = vert.fetchAddBound();
  const uint32_t _ptr_Storage_EngineInternal2 = vert.fetchAddBound();

  vert.setSpirvVersion(code.spirvVersion());
  auto fn = vert.end();
  fn.insert(spv::OpCapability,       {spv::CapabilityShader});
  fn.insert(spv::OpExtInstImport,    glslStd, "GLSL.std.450");
  fn.insert(spv::OpMemoryModel,      {spv::AddressingModelLogical, spv::MemoryModelGLSL450});
  fn.insert(spv::OpEntryPoint,       {spv::ExecutionModelVertex, main, 0x6E69616D, 0x0, gl_VertexIndex, vScratch});
  fn.insert(spv::OpSource,           {spv::SourceLanguageGLSL, 450});

  fn.insert(spv::OpName,             gl_VertexIndex,       "gl_VertexIndex");
  fn.insert(spv::OpName,             EngineInternal2_t,    "EngineInternal2");
  fn.insert(spv::OpMemberName,       EngineInternal2_t, 0, "grow");
  fn.insert(spv::OpMemberName,       EngineInternal2_t, 1, "heap");

  fn.insert(spv::OpDecorate,         {gl_VertexIndex, spv::DecorationBuiltIn, spv::BuiltInVertexIndex});

  fn.insert(spv::OpDecorate,         {_runtimearr_uint,     spv::DecorationArrayStride, 4});
  fn.insert(spv::OpDecorate,         {vScratch,             spv::DecorationDescriptorSet, 1});
  fn.insert(spv::OpDecorate,         {vScratch,             spv::DecorationBinding, 0});
  fn.insert(spv::OpDecorate,         {EngineInternal2_t,    spv::DecorationBlock});
  fn.insert(spv::OpMemberDecorate,   {EngineInternal2_t, 0, spv::DecorationOffset, 0});
  fn.insert(spv::OpMemberDecorate,   {EngineInternal2_t, 1, spv::DecorationOffset, 4});
  fn.insert(spv::OpMemberDecorate,   {EngineInternal2_t, 0, spv::DecorationNonWritable});
  fn.insert(spv::OpMemberDecorate,   {EngineInternal2_t, 1, spv::DecorationNonWritable});

  fn.insert(spv::OpTypeVoid,         {void_t});
  fn.insert(spv::OpTypeBool,         {bool_t});
  fn.insert(spv::OpTypeInt,          {uint_t,  32, 0});
  fn.insert(spv::OpTypeInt,          {int_t,   32, 1});
  fn.insert(spv::OpTypeFloat,        {float_t, 32});
  fn.insert(spv::OpTypeFunction,     {func_t, void_t});
  fn.insert(spv::OpTypeRuntimeArray, {_runtimearr_uint, uint_t});

  fn.insert(spv::OpTypePointer,      {_ptr_Input_int,    spv::StorageClassInput,         int_t});
  fn.insert(spv::OpTypePointer,      {_ptr_Output_uint,  spv::StorageClassOutput,        uint_t});
  fn.insert(spv::OpTypePointer,      {_ptr_Output_float, spv::StorageClassOutput,        float_t});
  fn.insert(spv::OpTypePointer,      {_ptr_Storage_uint, spv::StorageClassStorageBuffer, uint_t});

  std::unordered_map<uint32_t,uint32_t> typeRemap;
  for(auto& i:varying) {
    code.traverseType(i.second.type, [&](const libspirv::Bytecode::AccessChain* ids, uint32_t len){
      const auto&    op = *ids[len-1].type;
      const uint32_t id = op[1];
      if(typeRemap.find(id)!=typeRemap.end())
        return;

      switch(op.op()) {
        case spv::OpTypeVoid:
          typeRemap[id] = vert.OpTypeVoid(fn);
          break;
        case spv::OpTypeBool:
          typeRemap[id] = vert.OpTypeBool(fn);
          break;
        case spv::OpTypeInt:
          typeRemap[id] = vert.OpTypeInt(fn, op[2], op[3]);
          break;
        case spv::OpTypeFloat:
          typeRemap[id] = vert.OpTypeFloat(fn, op[2]);
          break;
        case spv::OpTypeVector:{
          uint32_t elt = typeRemap[op[2]];
          typeRemap[id] = vert.OpTypeVector(fn, elt, op[3]);
          break;
          }
        case spv::OpTypeMatrix:{
          uint32_t elt = typeRemap[op[2]];
          typeRemap[id] = vert.OpTypeMatrix(fn, elt, op[3]);
          break;
          }
        case spv::OpTypeArray:{
          uint32_t csize = 1;
          for(auto& i:code)
            if(i.op()==spv::OpConstant && i[2]==op[3]) {
              csize = i[3];
              break;
              }
          uint32_t uint_t = vert.OpTypeInt(fn, 32, false);
          uint32_t elt    = typeRemap[op[2]];
          uint32_t sz     = vert.OpConstant(fn, uint_t, csize);
          typeRemap[id]   = vert.OpTypeArray(fn, elt, sz);
          break;
          }
        case spv::OpTypeRuntimeArray:{
          uint32_t elt = typeRemap[op[2]];
          typeRemap[id] = vert.OpTypeRuntimeArray(fn, elt);
          break;
          }
        case spv::OpTypeStruct:{
          std::vector<uint32_t> idx(op.length()-2);
          for(size_t i=0; i<idx.size(); ++i)
            idx[i] = typeRemap[op[uint16_t(2+i)]];
          uint32_t ix = vert.OpTypeStruct(fn, idx.data(), idx.size());
          typeRemap[id] = ix;
          break;
          }
        case spv::OpTypePointer:{
          uint32_t elt = typeRemap[op[3]];
          for(auto& i:vert)
            if(i.op()==spv::OpTypeArray && i[1]==elt) {
              // remove arrayness
              elt = i[2];
              break;
              }
          assert(spv::StorageClass(op[2])==spv::StorageClassOutput);
          uint32_t ix  = vert.OpTypePointer(fn, spv::StorageClassOutput, elt);
          typeRemap[id] = ix;
          break;
          }
        default:
          assert(false);
        }
      }, libspirv::Bytecode::T_PostOrder);
    }

  fn.insert(spv::OpTypeStruct,       {EngineInternal2_t, uint_t, _runtimearr_uint}); // heap
  fn.insert(spv::OpTypePointer,      {_ptr_Storage_EngineInternal2, spv::StorageClassStorageBuffer, EngineInternal2_t});

  fn.insert(spv::OpVariable,         {_ptr_Input_int, gl_VertexIndex, spv::StorageClassInput});
  fn.insert(spv::OpVariable,         {_ptr_Storage_EngineInternal2, vScratch, spv::StorageClassStorageBuffer});
  for(auto& i:varying) {
    auto var  = vert.fetchAddBound();
    auto type = i.second.type;
    auto tx   = typeRemap[type];
    fn.insert(spv::OpVariable, {tx, var, spv::StorageClassOutput});
    i.second.vsVariable = var;
    }

  {
  fn = vert.findOpEntryPoint(spv::ExecutionModelVertex,"main");
  for(auto& i:varying) {
    fn.append(i.second.vsVariable);
    }
  fn = vert.end();
  }

  emitConstants(vert);
  fn = vert.end();

  fn.insert(spv::OpFunction,         {void_t, main, spv::FunctionControlMaskNone, func_t});
  fn.insert(spv::OpLabel,            {lblMain});
  {
  fn.insert(spv::OpLoad, {int_t, rg_VertexIndex, gl_VertexIndex});

  uint32_t seq = 0;
  for(auto& i:varying) {
    uint32_t varId = i.second.vsVariable;
    uint32_t type  = i.second.type;
    code.traverseType(type,[&](const libspirv::MutableBytecode::AccessChain* ids, uint32_t len) {
      if(!code.isBasicTypeDecl(ids[len-1].type->op()))
        return;
      if(len<2 || ids[1].index!=0)
        return; // [max_vertex] arrayness

      const uint32_t ptrR = vert.fetchAddBound();
      const uint32_t valR = vert.fetchAddBound();
      const uint32_t rDst = vert.fetchAddBound();
      fn.insert(spv::OpIAdd,        {uint_t, rDst, rg_VertexIndex, constants[seq]});
      fn.insert(spv::OpAccessChain, {_ptr_Storage_uint, ptrR, vScratch, constants[1], rDst});
      fn.insert(spv::OpLoad,        {uint_t, valR, ptrR});

      const uint32_t ptrL  = vert.fetchAddBound();
      const uint32_t rCast = vert.fetchAddBound();
      // NOTE: ids is pointer to array of T, we need only T
      const uint32_t ptrVar = vert.fetchAddBound();
      uint32_t chain[255] = {_ptr_Output_float, ptrL, varId};
      uint32_t chainSz    = 3;
      for(uint32_t i=2; i+1<len; ++i) {
        chain[chainSz] = constants[ids[i].index];
        ++chainSz;
        }

      // TODO: bool, maybe signed int
      if(ids[len-1].type->op()==spv::OpTypeFloat) {
        chain[0] = _ptr_Output_float;
        fn.insert(spv::OpAccessChain, chain, chainSz);
        fn.insert(spv::OpBitcast,     {float_t,  rCast, valR});
        fn.insert(spv::OpStore,       {ptrL, rCast});
        } else {
        chain[0] = _ptr_Output_uint;
        fn.insert(spv::OpLoad,  {uint_t, valR, ptrVar});
        fn.insert(spv::OpStore, {ptrL,   valR});
        }
      ++seq;
      });
    }
  }

  fn.insert(spv::OpReturn,      {});
  fn.insert(spv::OpFunctionEnd, {});

  {
  auto fn = vert.findSectionEnd(libspirv::Bytecode::S_Annotations);
  for(auto& i:varying) {
    uint32_t varId = i.second.vsVariable;
    uint32_t loc   = i.second.location;
    if(loc==uint32_t(-1))
      continue;
    fn.insert(spv::OpDecorate, {varId, spv::DecorationLocation, loc});
    }
  fn.insert(spv::OpDecorate, {typeRemap[gl_MeshPerVertexEXT], spv::DecorationBlock});
  for(auto& i:code) {
    if(i.op()==spv::OpMemberDecorate && i[1]==gl_MeshPerVertexEXT && i[3]==spv::DecorationBuiltIn) {
      fn.insert(spv::OpMemberDecorate, {typeRemap[i[1]], i[2], i[3], i[4]});
      }
    }

  fn = vert.findSectionEnd(libspirv::Bytecode::S_Debug);
  for(auto& i:code) {
    if(i.op()==spv::OpMemberName && i[1]==gl_MeshPerVertexEXT) {
      auto prevPos = vert.toOffset(*fn);
      fn.insert(spv::OpMemberName, &i[1], i.length()-1);

      auto prev = vert.fromOffset(prevPos);
      prev.set(1, typeRemap[i[1]]);
      }
    }
  }
  }

void MeshConverter::emitMVert(libspirv::MutableBytecode& comp) {
  bool ssboEmitted = false;

  iboAccess.clear();
  vboAccess.clear();

  comp.setSpirvVersion(code.spirvVersion());
  comp.fetchAddBound(code.bound()-comp.bound());

  const uint32_t gl_VertexIndex = vert.fetchAddBound();

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
      if(i[2]==spv::ExecutionModeLocalSize) {
        workGroupSize[0] = i[3];
        workGroupSize[1] = i[4];
        workGroupSize[2] = i[5];
        }
      if(i[2]==spv::ExecutionModeOutputVertices) {
        continue;
        }
      if(i[2]==spv::ExecutionModeOutputPrimitivesEXT) {
        continue;
        }
      if(i[2]==spv::ExecutionModeOutputTrianglesEXT) {
        continue;
        }
      if(i[2]==spv::ExecutionModeLocalSize) {
        continue;
        }
      }
    if(i.op()==spv::OpEntryPoint) {
      uint32_t ops[128] = {};
      ops[0] = spv::OpEntryPoint;
      ops[1] = spv::ExecutionModelVertex;
      ops[2] = i[2];       // main id
      ops[3] = 0x6E69616D; // "main"
      ops[4] = 0x0;

      uint32_t cnt = 5;
      for(size_t r=5; r<i.length(); ++r) {
        uint32_t id = i[uint16_t(r)];
        if(id==gl_PrimitiveTriangleIndicesEXT)
          continue;
        if(varying.find(id)!=varying.end())
          ;//continue;
        ops[cnt] = id; ++cnt;
        }

      gen.insert(i.op(),&ops[1],cnt-1);
      continue;
      }
    if(i.op()==spv::OpDecorate && i[2]==spv::DecorationBuiltIn) {
      if( i[3]==spv::BuiltInPrimitivePointIndicesEXT ||
          i[3]==spv::BuiltInPrimitiveLineIndicesEXT  ||
          i[3]==spv::BuiltInPrimitiveTriangleIndicesEXT ||
          //i[3]==spv::BuiltInNumWorkgroups ||
          i[3]==spv::BuiltInWorkgroupSize ||
          i[3]==spv::BuiltInWorkgroupId ||
          i[3]==spv::BuiltInLocalInvocationId ||
          i[3]==spv::BuiltInLocalInvocationIndex ||
          i[3]==spv::BuiltInGlobalInvocationId) {
        continue;
        }
      }
    if(i.op()==spv::OpMemberDecorate && i[3]==spv::DecorationBuiltIn) {
      // continue;
      }
    if(i.op()==spv::OpName) {
      auto name = std::string_view(reinterpret_cast<const char*>(&i[2]));
      if(name=="gl_MeshPerVertexEXT") {
        gen.insert(i.op(), i[1], "gl_PerVertex");
        continue;
        }

      auto off = gen.toOffset();
      gen.insert(i.op(),&i[1],i.length()-1);

      if(name.find("gl_")==0) {
        auto ix = comp.fromOffset(off);
        uint32_t gl = (*ix)[2];
        reinterpret_cast<char*>(&gl)[1] = '1';
        ix.set(2,gl);
        }
      continue;
      }
    if(i.op()==spv::OpMemberName) {
      auto name = std::string_view(reinterpret_cast<const char*>(&i[3]));
      if(name=="gl_Position") {
        continue;
        }
      auto off = gen.toOffset();
      gen.insert(i.op(),&i[1],i.length()-1);
      if(name.find("gl_")==0) {
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
        // flatten array
        auto ix   = comp.fromOffset(off);
        auto type = code.findOpType((*ix)[3]);

        if((*type).op()==spv::OpTypeArray)
          type = code.findOpType((*type)[2]);

        ix.set(2, spv::StorageClassOutput);
        ix.set(3, (*type)[1]);
        }
      else if(i[2]==spv::StorageClassWorkgroup) {
        auto ix = comp.fromOffset(off);
        ix.set(2,spv::StorageClassPrivate);
        }
      else if(i[2]==spv::StorageClassInput) {
        auto ix = comp.fromOffset(off);
        ix.set(2,spv::StorageClassPrivate);
        }
      continue;
      }
    if(i.op()==spv::OpVariable) {
      auto off = gen.toOffset();
      gen.insert(i.op(),&i[1],i.length()-1);
      if(i[3]==spv::StorageClassOutput) {
        // auto ix = comp.fromOffset(off);
        // ix.set(3,spv::StorageClassPrivate);
        }
      else if(i[3]==spv::StorageClassWorkgroup) {
        auto ix = comp.fromOffset(off);
        ix.set(3,spv::StorageClassPrivate);
        }
      else if(i[3]==spv::StorageClassInput) {
        auto ix = comp.fromOffset(off);
        ix.set(3,spv::StorageClassPrivate);
        }
      continue;
      }
    if(i.op()==spv::OpFunction) {
      if(!ssboEmitted) {
        emitConstants(comp);
        // emitEngSsbo(comp, spv::ExecutionModelVertex);
        ssboEmitted = true;
        gen = comp.end();
        }
      }
    if(i.op()==spv::OpSetMeshOutputsEXT) {
      continue;
      }
    if(i.op()==spv::OpAccessChain) {
      if(i[3]==gl_PrimitiveTriangleIndicesEXT) {
        iboAccess[i[2]] = code.toOffset(i);
        continue;
        }
      if(varying.find(i[3])!=varying.end()) {
        vboAccess[i[2]] = code.toOffset(i);

        auto off = gen.toOffset();
        gen.insert(i.op(),&i[1],i.length()-1);

        auto ix = comp.fromOffset(off);
        ix.evict(4);
        gen = comp.end();
        continue;
        }
      }
    if(i.op()==spv::OpStore) {
      auto chain = i[1];
      if(auto it=iboAccess.find(chain); it!=iboAccess.end()) {
        continue;
        }
      if(auto it=vboAccess.find(chain); it!=vboAccess.end()) {
        //continue;
        }
      }
    gen.insert(i.op(),&i[1],i.length()-1);
    }

  for(auto it = comp.begin(), end = comp.end(); it!=end; ++it) {
    auto& i = *it;
    if(i.op()==spv::OpName && (i[1]==gl_PrimitiveTriangleIndicesEXT)) {
      it.setToNop();
      }
    if(i.op()==spv::OpVariable && (i[2]==gl_PrimitiveTriangleIndicesEXT)) {
      it.setToNop();
      }
    }
  comp.removeNops();

  // declaration
  if(gl_VertexIndex!=0) {
    auto it = comp.findSectionEnd(libspirv::Bytecode::S_Types);
    const uint32_t int_t          = comp.OpTypeInt(it, 32, true);
    const uint32_t _ptr_Input_int = comp.OpTypePointer(it,spv::StorageClassInput, int_t);

    it = comp.findSectionEnd(libspirv::Bytecode::S_Debug);
    it.insert(spv::OpName, gl_VertexIndex, "gl_VertexIndex");

    it = comp.findSectionEnd(libspirv::Bytecode::S_Annotations);
    it.insert(spv::OpDecorate, {gl_VertexIndex, spv::DecorationBuiltIn, spv::BuiltInVertexIndex});

    it = comp.findOpEntryPoint(spv::ExecutionModelVertex, "main");
    it.append(gl_VertexIndex);

    it = comp.findSectionEnd(libspirv::Bytecode::S_Types);
    it.insert(spv::OpVariable, {_ptr_Input_int, gl_VertexIndex, spv::StorageClassInput});
    }

  // compute variables
  if(gl_VertexIndex!=0) {
    const uint32_t rg_VertexIndexI = vert.fetchAddBound();
    const uint32_t rg_VertexIndexU = vert.fetchAddBound();

    auto it = comp.findSectionEnd(libspirv::Bytecode::S_Types);
    const uint32_t wgSize            = workGroupSize[0]*workGroupSize[1]*workGroupSize[2];
    const uint32_t uint_t            = comp.OpTypeInt(it, 32, false);
    const uint32_t int_t             = comp.OpTypeInt(it, 32, true);
    const uint32_t _ptr_Private_uint = comp.OpTypePointer(it, spv::StorageClassPrivate, uint_t);
    const uint32_t constWg           = comp.OpConstant(it,uint_t,wgSize);

    it = comp.findOpEntryPoint(spv::ExecutionModelVertex,"main");
    uint32_t mainId = (*it)[2];

    it = comp.findFunction(mainId);
    it.insert(spv::OpLoad,    {int_t,  rg_VertexIndexI, gl_VertexIndex});
    it.insert(spv::OpBitcast, {uint_t, rg_VertexIndexU, rg_VertexIndexI});

    if(gl_LocalInvocationIndex!=0) {
      const uint32_t tmp0 = vert.fetchAddBound();
      const uint32_t tmp1 = vert.fetchAddBound();
      it.insert(spv::OpAccessChain, {_ptr_Private_uint, tmp0, gl_LocalInvocationIndex});
      it.insert(spv::OpUMod,        {uint_t, tmp1, rg_VertexIndexU, constWg});
      it.insert(spv::OpStore,       {tmp0, tmp1});
      }

    if(gl_LocalInvocationID!=0) {
      const uint32_t tmp0 = vert.fetchAddBound();
      const uint32_t tmp1 = vert.fetchAddBound();
      it.insert(spv::OpAccessChain, {_ptr_Private_uint, tmp0, gl_LocalInvocationID, constants[0]});
      it.insert(spv::OpUMod,        {uint_t, tmp1, rg_VertexIndexU, constWg});
      it.insert(spv::OpStore,       {tmp0, tmp1});
      }

    if(gl_WorkGroupID!=0) {
      const uint32_t tmp0 = vert.fetchAddBound();
      const uint32_t tmp1 = vert.fetchAddBound();
      it.insert(spv::OpAccessChain, {_ptr_Private_uint, tmp0, gl_WorkGroupID, constants[0]});
      it.insert(spv::OpUDiv,        {uint_t, tmp1, rg_VertexIndexU, constWg});
      it.insert(spv::OpStore,       {tmp0, tmp1});
      }
    }

  if(false && gl_LocalInvocationIndex==0) {
    auto it = comp.findSectionEnd(libspirv::Bytecode::S_Types);
    const uint32_t uint_t          = comp.OpTypeInt(it, 32, false);
    const uint32_t _ptr_Input_uint = comp.OpTypePointer(it,spv::StorageClassInput, uint_t);

    it = comp.findSectionEnd(libspirv::Bytecode::S_Types);
    gl_LocalInvocationIndex = comp.OpVariable(it, _ptr_Input_uint, spv::StorageClassInput);

    it = comp.findSectionEnd(libspirv::Bytecode::S_Debug);
    it.insert(spv::OpName, gl_LocalInvocationIndex, "gl_LocalInvocationIndex");

    it = comp.findSectionEnd(libspirv::Bytecode::S_Annotations);
    it.insert(spv::OpDecorate, {gl_LocalInvocationIndex, spv::DecorationBuiltIn, spv::BuiltInLocalInvocationIndex});

    it = comp.findOpEntryPoint(spv::ExecutionModelVertex,"main");
    it.append(gl_LocalInvocationIndex);
    }

  if(false && gl_WorkGroupID==0) {
    auto it = comp.findSectionEnd(libspirv::Bytecode::S_Types);
    const uint32_t uint_t           = comp.OpTypeInt(it, 32, false);
    const uint32_t uint3_t          = comp.OpTypeVector(it, uint_t, 3);
    const uint32_t _ptr_Input_uint3 = comp.OpTypePointer(it,spv::StorageClassInput, uint3_t);

    it = comp.findSectionEnd(libspirv::Bytecode::S_Types);
    gl_WorkGroupID = comp.OpVariable(it, _ptr_Input_uint3, spv::StorageClassInput);

    it = comp.findSectionEnd(libspirv::Bytecode::S_Debug);
    it.insert(spv::OpName, gl_WorkGroupID, "gl_WorkGroupID");

    it = comp.findSectionEnd(libspirv::Bytecode::S_Annotations);
    it.insert(spv::OpDecorate, {gl_WorkGroupID, spv::DecorationBuiltIn, spv::BuiltInWorkgroupId});

    it = comp.findOpEntryPoint(spv::ExecutionModelVertex,"main");
    it.append(gl_WorkGroupID);
    }

  // emitSetMeshOutputs(comp, engSetMesh);
  // emitEngMain(comp, myMain);
  }
