#include "meshconverter.h"
#include "libspirv/GLSL.std.450.h"

#include <cassert>
#include <iostream>
#include <unordered_set>

MeshConverter::MeshConverter(libspirv::MutableBytecode& code)
  : code(code) {
  }

void MeshConverter::exec() {
  avoidReservedFixup();

  for(auto it = code.begin(), end = code.end(); it!=end; ++it) {
    auto& i = *it;
    if(i.op()==spv::OpDecorate && i[2]==spv::DecorationBuiltIn) {
      if(i[3]==spv::BuiltInPrimitiveCountNV) {
        idPrimitiveCountNV = i[1];
        it.setToNop();
        }
      if(i[3]==spv::BuiltInPrimitiveIndicesNV) {
        idPrimitiveIndicesNV = i[1];
        it.setToNop();
        }

      if(i[3]==spv::BuiltInLocalInvocationId) {
        idLocalInvocationId = i[1];
        }
      if(i[3]==spv::BuiltInGlobalInvocationId) {
        idGlobalInvocationId = i[1];
        }
      if(i[3]==spv::BuiltInWorkgroupId) {
        idWorkGroupID = i[1];
        }
      }
    if(i.op()==spv::OpExtInstImport) {
      std::string_view name = reinterpret_cast<const char*>(&i[2]);
      if(name=="GLSL.std.450")
        std450Import = i[1];
      }
    if(i.op()==spv::OpExecutionMode &&
       (i[2]==spv::ExecutionModeOutputVertices || i[2]==spv::ExecutionModeOutputLinesNV || i[2]==spv::ExecutionModeOutputPrimitivesNV ||
        i[2]==spv::ExecutionModeOutputTrianglesNV)) {
      it.setToNop();
      }
    if(i.op()==spv::OpExtension) {
      std::string_view name = reinterpret_cast<const char*>(&i[1]);
      if(name=="SPV_NV_mesh_shader")
        it.setToNop();
      }
    if(i.op()==spv::OpCapability && i[1]==spv::CapabilityMeshShadingNV) {
      it.set(1,spv::CapabilityShader);
      }
    }

  // TODO: import std450Import automatically
  assert(std450Import!=uint32_t(-1));

  // gl_MeshPerVertexNV block
  removeMultiview(code);
  for(auto it = code.begin(), end = code.end(); it!=end; ++it) {
    auto& i = *it;
    if(i.op()==spv::OpDecorate && i[2]==spv::DecorationBlock) {
      it.setToNop();
      continue;
      }
    if(i.op()==spv::OpDecorate && i[2]==spv::DecorationLocation) {
      it.setToNop();
      continue;
      }
    if(i.op()==spv::OpMemberDecorate && i[3]==spv::DecorationBuiltIn) {
      it.setToNop();
      continue;
      }
    if(i.op()==spv::OpMemberDecorate && i[3]==spv::DecorationPerViewNV) {
      it.setToNop();
      continue;
      }
    }

  for(auto it = code.begin(), end = code.end(); it!=end; ++it) {
    auto& i = *it;
    if((i.op()==spv::OpVariable && i[3]==spv::StorageClassOutput)) {
      if(i[2]!=idPrimitiveCountNV &&
         i[2]!=idPrimitiveIndicesNV) {
        VarItm it;
        it.type = i[1];
        outVar.insert({i[2],it});
        }
      it.set(3, spv::StorageClassWorkgroup);
      }
    if((i.op()==spv::OpTypePointer && i[2]==spv::StorageClassOutput)) {
      it.set(2, spv::StorageClassWorkgroup);
      }
    }

  // meslet entry-point
  uint32_t idMain = -1;
  {
  auto ep = code.findOpEntryPoint(spv::ExecutionModelMeshNV,"main");
  if(ep!=code.end()) {
    idMain = (*ep)[2];
    ep.set(1,spv::ExecutionModelGLCompute);
    // remove in/out variables
    if(code.spirvVersion()<0x00010400) {
      /* Starting with version 1.4, the interface’s storage classes are all
       * storage classes used in declaring all global variables
       * referenced by the entry point’s call tree.
       * So on newer spir-v - there is no point of patching entry-point
       */

      auto i  = *ep;
      auto ix = i;
      std::string_view name = reinterpret_cast<const char*>(&i[3]);
      ix.len = 3+(name.size()+1+3)/4;

      for(uint16_t r=ix.length(); r<i.length(); ++r) {
        auto cp = (*ep)[r];
        if(cp!=idLocalInvocationId &&
           cp!=idGlobalInvocationId &&
           cp!=idWorkGroupID) {
          continue;
          }
        ep.set(ix.len,cp);
        ix.len++;
        }
      for(uint16_t r=ix.length(); r<i.length(); ++r) {
        ep.set(r,libspirv::Bytecode::OpCode{0,1});
        }
      ep.set(0,ix);
      }
    }
  }

  if(idLocalInvocationId==-1) {
    auto fn = code.findSectionEnd(libspirv::Bytecode::S_Types);
    const uint32_t tUInt             = code.OpTypeInt(fn,32,false);
    const uint32_t v3uint            = code.OpTypeVector(fn,tUInt,3);
    const uint32_t _ptr_Input_v3uint = code.OpTypePointer(fn,spv::StorageClassInput,v3uint);

    idLocalInvocationId = code.fetchAddBound();
    fn.insert(spv::OpVariable, {_ptr_Input_v3uint, idLocalInvocationId, spv::StorageClassInput});

    fn = code.findSectionEnd(libspirv::Bytecode::S_Debug);
    fn.insert(spv::OpName, idLocalInvocationId, "gl_LocalInvocationID");

    fn = code.findSectionEnd(libspirv::Bytecode::S_Annotations);
    fn.insert(spv::OpDecorate, {idLocalInvocationId, spv::DecorationBuiltIn, spv::BuiltInLocalInvocationId});

    auto ep = code.findOpEntryPoint(spv::ExecutionModelGLCompute,"main");
    if(ep!=code.end()) {
      ep.append(idLocalInvocationId);
      }
    }

  injectCountingPass(idMain);
  // nops are not valid to have outsize of block
  code.removeNops();

  generateVs();
  }

void MeshConverter::generateVs() {
  const auto glslStd  = vert.fetchAddBound();
  const auto main     = vert.fetchAddBound();

  const auto texcoord = vert.fetchAddBound();
  const auto outColor = vert.fetchAddBound();

  auto fn = vert.end();
  fn.insert(spv::OpCapability,    {spv::CapabilityShader});
  fn.insert(spv::OpExtInstImport, glslStd, "GLSL.std.450");
  fn.insert(spv::OpMemoryModel,   {spv::AddressingModelLogical, spv::MemoryModelGLSL450});
  fn.insert(spv::OpEntryPoint,    {spv::ExecutionModelFragment, main, 0x4D00, texcoord, outColor});
  //fn.set(3,"main");
  fn.insert(spv::OpSource,        {spv::SourceLanguageGLSL, 450});
  fn.insert(spv::OpName,          texcoord, "texcoord");
  fn.insert(spv::OpName,          outColor, "outColor");
  }

void MeshConverter::avoidReservedFixup() {
  // avoid _RESERVED_IDENTIFIER_FIXUP_ spit of spirv-cross
  for(auto it = code.begin(), end = code.end(); it!=end; ++it) {
    auto& i = *it;
    if(i.op()!=spv::OpName && i.op()!=spv::OpMemberName)
      continue;
    size_t at = (i.op()==spv::OpName ? 2 : 3);
    std::string_view name = reinterpret_cast<const char*>(&i[at]);
    if(name=="gl_MeshVerticesNV" ||
       name=="gl_PrimitiveCountNV" ||
       name=="gl_PrimitiveIndicesNV" ||

       name=="gl_MeshPerVertexNV" ||
       name=="gl_Position" ||
       name=="gl_PointSize" ||
       name=="gl_ClipDistance" ||
       name=="gl_CullDistance" ||
       name=="gl_PositionPerViewNV"  ||
       name=="gl_PositionPerViewNV" ) {
      uint32_t w0 = i[at];
      auto* b = reinterpret_cast<char*>(&w0);
      b[0] = 'g';
      b[1] = '1';
      it.set(at,w0);
      }
    }
  }

void MeshConverter::injectLoop(libspirv::MutableBytecode::Iterator& fn,
                               uint32_t varI, uint32_t begin, uint32_t end, uint32_t inc,
                               std::function<void(libspirv::MutableBytecode::Iterator& fn)> body) {
  auto  fnT  = code.findSectionEnd(libspirv::Bytecode::S_Types);
  auto  off0 = fnT.toOffset();

  const uint32_t tBool  = code.OpTypeBool(fnT);
  const uint32_t tUInt  = code.OpTypeInt (fnT,32,false);

  const auto     diff   = fnT.toOffset()-off0;
  fn = code.fromOffset(fn.toOffset()+diff);// TODO: test it properly

  fn.insert(spv::OpStore, {varI, begin});
  const uint32_t loopBegin = code.fetchAddBound();
  const uint32_t loopBody  = code.fetchAddBound();
  const uint32_t loopEnd   = code.fetchAddBound();
  const uint32_t loopStep  = code.fetchAddBound();
  const uint32_t loopCond  = code.fetchAddBound();
  const uint32_t rgCond    = code.fetchAddBound();
  const uint32_t rgI0      = code.fetchAddBound();
  const uint32_t rgI1      = code.fetchAddBound();
  const uint32_t rgI2      = code.fetchAddBound();

  fn.insert(spv::OpBranch,            {loopBegin});
  fn.insert(spv::OpLabel,             {loopBegin});

  fn.insert(spv::OpLoopMerge,         {loopEnd, loopStep, spv::SelectionControlMaskNone});
  fn.insert(spv::OpBranch,            {loopCond});

  fn.insert(spv::OpLabel,             {loopCond});
  fn.insert(spv::OpLoad,              {tUInt, rgI0, varI});
  fn.insert(spv::OpULessThan,         {tBool, rgCond, rgI0, end});
  fn.insert(spv::OpBranchConditional, {rgCond, loopBody, loopEnd});

  fn.insert(spv::OpLabel,             {loopBody});
  body(fn);
  fn.insert(spv::OpBranch,            {loopStep});

  fn.insert(spv::OpLabel,             {loopStep});
  fn.insert(spv::OpLoad,              {tUInt, rgI1, varI});
  fn.insert(spv::OpIAdd,              {tUInt, rgI2, rgI1, inc});
  fn.insert(spv::OpStore,             {varI,  rgI2});
  fn.insert(spv::OpBranch,            {loopBegin});

  fn.insert(spv::OpLabel,             {loopEnd});
  }

void MeshConverter::injectCountingPass(const uint32_t idMainFunc) {
  auto fn = code.findSectionEnd(libspirv::Bytecode::S_Types);

  const uint32_t tVoid                = code.OpTypeVoid(fn);
  const uint32_t tBool                = code.OpTypeBool(fn);
  const uint32_t tUInt                = code.OpTypeInt(fn,32,false);
  const uint32_t tSInt                = code.OpTypeInt(fn,32,true);
  const uint32_t tFloat               = code.OpTypeFloat(fn,32);
  const uint32_t tVoidFn              = code.OpTypeFunction(fn,tVoid);
  const uint32_t _ptr_Input_uint      = code.OpTypePointer(fn, spv::StorageClassInput,     tUInt);
  const uint32_t _ptr_Uniform_uint    = code.OpTypePointer(fn, spv::StorageClassUniform,   tUInt);
  const uint32_t _ptr_Workgroup_uint  = code.OpTypePointer(fn, spv::StorageClassWorkgroup, tUInt);
  const uint32_t _ptr_Function_uint   = code.OpTypePointer(fn, spv::StorageClassFunction,  tUInt);
  const uint32_t _ptr_Workgroup_float = code.OpTypePointer(fn, spv::StorageClassWorkgroup, tFloat);

  const uint32_t _runtimearr_uint    = code.OpTypeRuntimeArray(fn, tUInt);
  const uint32_t EngineInternal0     = code.OpTypeStruct (fn, {tUInt, tUInt, tUInt, tSInt, tUInt, tUInt, tUInt, tUInt});
  const uint32_t EngineInternal1     = code.OpTypeStruct (fn, {tUInt, tUInt, tUInt, _runtimearr_uint});
  const uint32_t EngineInternal2     = code.OpTypeStruct (fn, {tUInt, _runtimearr_uint});

  const uint32_t _ptr_Uniform_EngineInternal0 = code.OpTypePointer(fn,spv::StorageClassUniform, EngineInternal0);
  const uint32_t _ptr_Uniform_EngineInternal1 = code.OpTypePointer(fn,spv::StorageClassUniform, EngineInternal1);
  const uint32_t _ptr_Uniform_EngineInternal2 = code.OpTypePointer(fn,spv::StorageClassUniform, EngineInternal2);

  const uint32_t const0   = code.OpConstant(fn,tUInt,0);
  const uint32_t const1   = code.OpConstant(fn,tUInt,1);
  const uint32_t const2   = code.OpConstant(fn,tUInt,2);
  const uint32_t const3   = code.OpConstant(fn,tUInt,3);
  const uint32_t const10  = code.OpConstant(fn,tUInt,10);
  const uint32_t const18  = code.OpConstant(fn,tUInt,18);
  const uint32_t const128 = code.OpConstant(fn,tUInt,128);
  const uint32_t const264 = code.OpConstant(fn,tUInt,264);

  std::unordered_map<uint32_t,uint32_t> constants;
  uint32_t varCount = 0;
  {
  for(auto& i:outVar) {
    // preallocate indexes
    code.traverseType(i.second.type,[&](const libspirv::MutableBytecode::AccessChain* ids, uint32_t len) {
      for(uint32_t i=0; i<len; ++i) {
        uint32_t x = ids[i].index;
        if(constants.find(x)==constants.end())
          constants[x] = code.OpConstant(fn,tUInt, x);
        }
      if(constants.find(varCount)==constants.end())
        constants[varCount] = code.OpConstant(fn,tUInt, varCount);
      ++varCount;
      });
    }
  }

  const uint32_t topology = code.OpConstant(fn,tUInt,3);
  const uint32_t varSize  = code.OpConstant(fn,tUInt,varCount);

  const uint32_t vEngine0 = code.OpVariable(fn, _ptr_Uniform_EngineInternal0, spv::StorageClassUniform);
  const uint32_t vEngine1 = code.OpVariable(fn, _ptr_Uniform_EngineInternal1, spv::StorageClassUniform);
  const uint32_t vEngine2 = code.OpVariable(fn, _ptr_Uniform_EngineInternal2, spv::StorageClassUniform);

  fn = code.findSectionEnd(libspirv::Bytecode::S_Annotations);
  fn.insert(spv::OpDecorate, {_runtimearr_uint, spv::DecorationArrayStride, 4});

  fn.insert(spv::OpDecorate, {EngineInternal0, spv::DecorationBufferBlock});
  fn.insert(spv::OpDecorate, {vEngine0, spv::DecorationDescriptorSet, 1});
  fn.insert(spv::OpDecorate, {vEngine0, spv::DecorationBinding, 0});
  for(uint32_t i=0; i<8; ++i)
    fn.insert(spv::OpMemberDecorate, {EngineInternal0, i, spv::DecorationOffset, i*4});

  fn.insert(spv::OpDecorate, {EngineInternal1, spv::DecorationBufferBlock});
  fn.insert(spv::OpDecorate, {vEngine1, spv::DecorationDescriptorSet, 1});
  fn.insert(spv::OpDecorate, {vEngine1, spv::DecorationBinding, 1});
  for(uint32_t i=0; i<4; ++i)
    fn.insert(spv::OpMemberDecorate, {EngineInternal1, i, spv::DecorationOffset, i*4});

  fn.insert(spv::OpDecorate, {EngineInternal2, spv::DecorationBufferBlock});
  fn.insert(spv::OpDecorate, {vEngine2, spv::DecorationDescriptorSet, 1});
  fn.insert(spv::OpDecorate, {vEngine2, spv::DecorationBinding, 2});
  for(uint32_t i=0; i<2; ++i)
    fn.insert(spv::OpMemberDecorate, {EngineInternal2, i, spv::DecorationOffset, i*4});

  fn = code.findSectionEnd(libspirv::Bytecode::S_Debug);
  fn.insert(spv::OpName,       EngineInternal0,    "VkDrawIndexedIndirectCommand");
  fn.insert(spv::OpMemberName, EngineInternal0, 0, "indexCount");
  fn.insert(spv::OpMemberName, EngineInternal0, 1, "instanceCount");
  fn.insert(spv::OpMemberName, EngineInternal0, 2, "firstIndex");
  fn.insert(spv::OpMemberName, EngineInternal0, 3, "vertexOffset");
  fn.insert(spv::OpMemberName, EngineInternal0, 4, "firstInstance");
  fn.insert(spv::OpMemberName, EngineInternal0, 5, "self");
  fn.insert(spv::OpMemberName, EngineInternal0, 6, "vboOffset");
  fn.insert(spv::OpMemberName, EngineInternal0, 7, "padd1");

  fn.insert(spv::OpName,       EngineInternal1,    "EngineInternal1");
  fn.insert(spv::OpMemberName, EngineInternal1, 0, "grow");
  fn.insert(spv::OpMemberName, EngineInternal1, 1, "dispatchY");
  fn.insert(spv::OpMemberName, EngineInternal1, 2, "dispatchZ");
  fn.insert(spv::OpMemberName, EngineInternal1, 3, "desc");

  fn.insert(spv::OpName,       EngineInternal2,    "EngineInternal2");
  fn.insert(spv::OpMemberName, EngineInternal2, 0, "grow");
  fn.insert(spv::OpMemberName, EngineInternal2, 1, "heap");

  // engine-level main
  fn = code.end();
  const uint32_t engineMain = code.fetchAddBound();
  const uint32_t lblMain    = code.fetchAddBound();
  fn.insert(spv::OpFunction, {tVoid, engineMain, spv::FunctionControlMaskNone, tVoidFn});
  fn.insert(spv::OpLabel, {lblMain});

  const uint32_t varI   = code.OpVariable(fn, _ptr_Function_uint, spv::StorageClassFunction);
  const uint32_t maxInd = code.OpVariable(fn, _ptr_Function_uint, spv::StorageClassFunction);

  const uint32_t tmp0 = code.fetchAddBound();
  fn.insert(spv::OpFunctionCall, {tVoid, tmp0, idMainFunc});

  fn.insert(spv::OpMemoryBarrier,  {const1, const264});         // memoryBarrierShared() // needed?
  fn.insert(spv::OpControlBarrier, {const2, const2, const264}); // barrier()

  const uint32_t primCount = code.fetchAddBound();
  fn.insert(spv::OpLoad, {tUInt, primCount, idPrimitiveCountNV});

  // gl_PrimitiveCountNV <= 0
  {
    const uint32_t cond0 = code.fetchAddBound();
    fn.insert(spv::OpULessThanEqual, {tBool, cond0, primCount, const0});

    const uint32_t condBlockBegin = code.fetchAddBound();
    const uint32_t condBlockEnd   = code.fetchAddBound();
    fn.insert(spv::OpSelectionMerge,    {condBlockEnd, spv::SelectionControlMaskNone});
    fn.insert(spv::OpBranchConditional, {cond0, condBlockBegin, condBlockEnd});
    fn.insert(spv::OpLabel,             {condBlockBegin});
    fn.insert(spv::OpReturn,            {});
    fn.insert(spv::OpLabel,             {condBlockEnd});
  }

  // gl_LocalInvocationID.x != 0
  {
    const uint32_t ptrInvocationIdX = code.fetchAddBound();
    fn.insert(spv::OpAccessChain, {_ptr_Input_uint, ptrInvocationIdX, idLocalInvocationId, const0});
    const uint32_t invocationId = code.fetchAddBound();
    fn.insert(spv::OpLoad, {tUInt, invocationId, ptrInvocationIdX});
    const uint32_t cond1 = code.fetchAddBound();
    fn.insert(spv::OpINotEqual, {tBool, cond1, invocationId, const0});

    const uint32_t condBlockBegin = code.fetchAddBound();
    const uint32_t condBlockEnd   = code.fetchAddBound();
    fn.insert(spv::OpSelectionMerge,    {condBlockEnd, spv::SelectionControlMaskNone});
    fn.insert(spv::OpBranchConditional, {cond1, condBlockBegin, condBlockEnd});
    fn.insert(spv::OpLabel,             {condBlockBegin});
    fn.insert(spv::OpReturn,            {});
    fn.insert(spv::OpLabel,             {condBlockEnd});
  }

  // indSize = gl_PrimitiveCountNV*3
  const uint32_t indSize = code.fetchAddBound();
  fn.insert(spv::OpIMul, {tUInt, indSize, primCount, topology});

  // Max Vertex. Assume empty output
  fn.insert(spv::OpStore, {maxInd, const0});
  injectLoop(fn, varI, const0, indSize, const1, [&](libspirv::MutableBytecode::Iterator& fn) {
    const uint32_t rI = code.fetchAddBound();
    fn.insert(spv::OpLoad, {tUInt, rI, varI});

    const uint32_t ptrIndicesNV = code.fetchAddBound();
    fn.insert(spv::OpAccessChain, {_ptr_Workgroup_uint, ptrIndicesNV, idPrimitiveIndicesNV, rI});

    const uint32_t rInd = code.fetchAddBound();
    fn.insert(spv::OpLoad, {tUInt, rInd, ptrIndicesNV});

    uint32_t a = code.fetchAddBound();
    fn.insert(spv::OpLoad, {tUInt, a, maxInd});

    uint32_t b = code.fetchAddBound();
    fn.insert(spv::OpExtInst, {tUInt, b, std450Import, GLSLstd450UMax, a, rInd});

    fn.insert(spv::OpStore, {maxInd, b});
    });

  // Fair counting of indices
  const uint32_t rMaxInd = code.fetchAddBound();
  fn.insert(spv::OpLoad, {tUInt, rMaxInd, maxInd});
  const uint32_t maxVertex = code.fetchAddBound();
  fn.insert(spv::OpIAdd, {tUInt, maxVertex, rMaxInd, const1});

  const uint32_t maxVar = code.fetchAddBound();
  fn.insert(spv::OpIMul, {tUInt, maxVar, maxVertex, varSize});
  {
  const uint32_t ptrCmdDest = code.fetchAddBound();
  fn.insert(spv::OpAccessChain, {_ptr_Uniform_uint, ptrCmdDest, vEngine0, const0});
  const uint32_t cmdDest  = code.fetchAddBound();
  fn.insert(spv::OpAtomicIAdd, {tUInt, cmdDest, ptrCmdDest, const1/*scope*/, const0/*semantices*/, indSize});

  const uint32_t ptrCmdDest2 = code.fetchAddBound();
  fn.insert(spv::OpAccessChain, {_ptr_Uniform_uint, ptrCmdDest2, vEngine0, const1});
  const uint32_t cmdDest2  = code.fetchAddBound();
  fn.insert(spv::OpAtomicIAdd, {tUInt, cmdDest2, ptrCmdDest2, const1/*scope*/, const0/*semantices*/, maxVar});
  }

  const uint32_t heapAllocSz = code.fetchAddBound();
  fn.insert(spv::OpIAdd, {tUInt, heapAllocSz, maxVar, indSize});

  const uint32_t ptrHeapDest = code.fetchAddBound();
  fn.insert(spv::OpAccessChain, {_ptr_Uniform_uint, ptrHeapDest, vEngine2, const0});
  const uint32_t heapDest  = code.fetchAddBound();
  fn.insert(spv::OpAtomicIAdd, {tUInt, heapDest, ptrHeapDest, const1/*scope*/, const0/*semantices*/, heapAllocSz});
  // uint varDest = indDest + indSize;
  const uint32_t varDest  = code.fetchAddBound();
  fn.insert(spv::OpIAdd, {tUInt, varDest, heapDest, indSize});

  // uint meshDest = atomicAdd(mesh.grow, 1)*3;
  const uint32_t ptrMeshDest = code.fetchAddBound();
  fn.insert(spv::OpAccessChain, {_ptr_Uniform_uint, ptrMeshDest, vEngine1, const0});
  const uint32_t meshDestRaw  = code.fetchAddBound();
  fn.insert(spv::OpAtomicIAdd, {tUInt, meshDestRaw, ptrMeshDest, const1/*scope*/, const0/*semantices*/, const1});
  const uint32_t meshDest = code.fetchAddBound();
  fn.insert(spv::OpIMul, {tUInt, meshDest, meshDestRaw, const3});

  // Writeout indexes
  injectLoop(fn, varI, const0, indSize, const1, [&](libspirv::MutableBytecode::Iterator& fn) {
    const uint32_t rI = code.fetchAddBound();
    fn.insert(spv::OpLoad, {tUInt, rI, varI});
    const uint32_t rDst = code.fetchAddBound();
    fn.insert(spv::OpIAdd, {tUInt, rDst, rI, heapDest});
    const uint32_t ptrHeap = code.fetchAddBound();
    fn.insert(spv::OpAccessChain, {_ptr_Uniform_uint, ptrHeap, vEngine2, const1, rDst});

    const uint32_t ptrIndicesNV = code.fetchAddBound();
    fn.insert(spv::OpAccessChain, {_ptr_Workgroup_uint, ptrIndicesNV, idPrimitiveIndicesNV, rI});

    const uint32_t rInd = code.fetchAddBound();
    fn.insert(spv::OpLoad, {tUInt, rInd, ptrIndicesNV});

    fn.insert(spv::OpStore, {ptrHeap, rInd});
    });

  // Writeout varying
  {
  injectLoop(fn, varI, const0, maxVertex, const1, [&](libspirv::MutableBytecode::Iterator& fn) {
    // uint at = varDest + i*varSize;
    const uint32_t rI = code.fetchAddBound();
    fn.insert(spv::OpLoad, {tUInt, rI, varI});
    const uint32_t rTmp0 = code.fetchAddBound();
    fn.insert(spv::OpIMul, {tUInt, rTmp0, rI, varSize});
    const uint32_t rAt = code.fetchAddBound();
    fn.insert(spv::OpIAdd, {tUInt, rAt, rTmp0, varDest});

    libspirv::MutableBytecode b;
    auto block = b.end();
    uint32_t seq  = 0;
    for(auto& i:outVar) {
      uint32_t type = i.second.type;
      code.traverseType(type,[&](const libspirv::MutableBytecode::AccessChain* ids, uint32_t len) {
        // at += memberId
        const uint32_t rDst = code.fetchAddBound();
        block.insert(spv::OpIAdd, {tUInt, rDst, rAt, constants[seq]});
        ++seq;
        const uint32_t ptrHeap = code.fetchAddBound();
        block.insert(spv::OpAccessChain, {_ptr_Uniform_uint, ptrHeap, vEngine2, const1, rDst});

        // NOTE: ids is pointer to array of X, we need only X
        const uint32_t ptrVar = code.fetchAddBound();
        uint32_t chain[255] = {_ptr_Workgroup_float, ptrVar, i.first, rI};
        uint32_t chainSz    = 4;
        for(uint32_t i=2; i+1<len; ++i) {
          chain[chainSz] = constants[ids[i].index];
          ++chainSz;
          }

        block.insert(spv::OpAccessChain, chain, chainSz);
        const uint32_t rVal = code.fetchAddBound();
        if(ids[len-1].type->op()==spv::OpTypeFloat) {
          const uint32_t rCast = code.fetchAddBound();
          chain[0] = _ptr_Workgroup_float;
          block.insert(spv::OpLoad, {tFloat, rVal, ptrVar});
          block.insert(spv::OpBitcast, {tUInt, rCast, rVal});
          block.insert(spv::OpStore, {ptrHeap, rCast});
          } else {
          chain[0] = _ptr_Workgroup_uint;
          block.insert(spv::OpLoad, {tUInt, rVal, ptrVar});
          block.insert(spv::OpStore, {ptrHeap, rVal});
          }
        });
      }
    fn.insert(b);
    });
  }

  // Writeout meshlet descriptor
  {
    const uint32_t ptrHeap0 = code.fetchAddBound();
    fn.insert(spv::OpAccessChain, {_ptr_Uniform_uint, ptrHeap0, vEngine1, const3, meshDest});
    fn.insert(spv::OpStore, {ptrHeap0, const264}); // TODO: writeout self-id

    const uint32_t dest1 = code.fetchAddBound();
    fn.insert(spv::OpIAdd, {tUInt, dest1, meshDest, const1});
    const uint32_t ptrHeap1 = code.fetchAddBound();
    fn.insert(spv::OpAccessChain, {_ptr_Uniform_uint, ptrHeap1, vEngine1, const3, dest1});
    fn.insert(spv::OpStore, {ptrHeap1, heapDest});

    const uint32_t dest2 = code.fetchAddBound();
    fn.insert(spv::OpIAdd, {tUInt, dest2, meshDest, const2});
    const uint32_t ptrHeap2 = code.fetchAddBound();
    fn.insert(spv::OpAccessChain, {_ptr_Uniform_uint, ptrHeap2, vEngine1, const3, dest2});

    const uint32_t tmp0 = code.fetchAddBound();
    fn.insert(spv::OpShiftLeftLogical, {tUInt, tmp0, maxVertex, const10});

    const uint32_t tmp1 = code.fetchAddBound();
    fn.insert(spv::OpShiftLeftLogical, {tUInt, tmp1, varSize, const18});

    const uint32_t tmp2 = code.fetchAddBound();
    fn.insert(spv::OpBitwiseOr, {tUInt, tmp2, tmp1, tmp0});

    const uint32_t tmp3 = code.fetchAddBound();
    fn.insert(spv::OpBitwiseOr, {tUInt, tmp3, tmp2, indSize});

    fn.insert(spv::OpStore, {ptrHeap2, tmp3});
  }

  fn.insert(spv::OpReturn, {});
  fn.insert(spv::OpFunctionEnd, {});

  // entry-point
  replaceEntryPoint(idMainFunc,engineMain);

  // debug info
  fn = code.findSectionEnd(libspirv::Bytecode::S_Debug);
  fn.insert(spv::OpName, engineMain, "main");
  fn.insert(spv::OpName, varI,       "i");
  }

void MeshConverter::replaceEntryPoint(const uint32_t idMainFunc, const uint32_t idEngineMain) {
  auto ep = code.findOpEntryPoint(spv::ExecutionModelGLCompute,"main");
  assert(ep!=code.end());
  ep.set(2,idEngineMain);

  for(auto it = code.begin(), end = code.end(); it!=end; ++it) {
    auto& i = *it;
    if(i.op()==spv::OpEntryPoint && i[2]==idMainFunc) {
      it.setToNop();
      }
    if(i.op()==spv::OpName && i[1]==idMainFunc) {
      // to not confuse spirv-cross
      it.setToNop();
      }
    if(i.op()==spv::OpExecutionMode && i[1]==idMainFunc) {
      it.set(1,idEngineMain);
      }
    }
  }

void MeshConverter::removeMultiview(libspirv::MutableBytecode& code) {
  uint32_t idMeshPerVertexNV = uint32_t(-1);
  for(auto it = code.begin(), end = code.end(); it!=end; ++it) {
    auto& i = *it;
    if(i.op()==spv::OpMemberDecorate && i[3]==spv::DecorationPerViewNV) {
      idMeshPerVertexNV = i[1];
      break;
      }
    }

  if(idMeshPerVertexNV==uint32_t(-1))
    return;

  std::unordered_set<uint32_t> perView;
  for(auto it = code.begin(), end = code.end(); it!=end; ++it) {
    auto& i = *it;
    if(i.op()==spv::OpMemberDecorate && i[3]==spv::DecorationPerViewNV) {
      perView.insert(i[2]);
      continue;
      }
    }

  for(auto it = code.begin(), end = code.end(); it!=end; ++it) {
    auto& i = *it;
    if((i.op()!=spv::OpMemberDecorate && i.op()!=spv::OpMemberName) || i[1]!=idMeshPerVertexNV)
      continue;
    if(perView.find(i[2])==perView.end())
      continue;
    it.setToNop();
    }

  for(auto it = code.begin(), end = code.end(); it!=end; ++it) {
    auto& i = *it;
    if(i.op()!=spv::OpTypeStruct)
      continue;
    if(idMeshPerVertexNV!=i[1])
      continue;

    uint16_t argc = 1;
    uint32_t args[16] = {};
    args[0] = i[1];
    for(size_t r=2; r<i.length(); ++r) {
      if(perView.find(r-2)!=perView.end())
        continue;
      args[argc] = i[r];
      ++argc;
      }
    it.setToNop();
    it.insert(spv::OpTypeStruct,args,argc);
    break;
    }
  }
